#include "Dialogue/MADialogueSubsystem.h"

#include "Dialogue/MADialogueComponent.h"
#include "LLM/MALLMSettings.h"
#include "LLM/MALLMStreamingClient.h"
#include "Narrative/MANarrativeSubsystem.h"
#include "Narrative/MAQuestRules.h"
#include "Player/MAPlayerController.h"
#include "Player/MAPlayerState.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogMADialogue, Log, All);

namespace
{
	// 单条玩家消息的长度上限（防恶意客户端灌 token）。
	constexpr int32 GMaxPlayerMessageLen = 500;
	// 攒到这个字符数就不等 flush 间隔直接发（保持首字延迟低）。
	constexpr int32 GFlushImmediatelyAt = 512;

	FString BuildSystemPrompt(const UMADialogueComponent& Npc)
	{
		FString Prompt = UMALLMSettings::Get()->SystemPromptTemplate;
		Prompt.ReplaceInline(TEXT("{NpcName}"), *Npc.NpcName);
		Prompt.ReplaceInline(TEXT("{Persona}"), *Npc.Persona);
		return Prompt;
	}
}

void UMADialogueSubsystem::StartSession(AMAPlayerController* PC, UMADialogueComponent* Npc)
{
	if (!PC || !Npc || !Npc->GetOwner())
	{
		return;
	}

	// 任务进行中 NPC 离场 —— 恶意客户端跳过本地隐身检查也在这里被拦。
	if (Npc->GetOwner()->IsHidden())
	{
		return;
	}

	// 服务器权威距离验证（客户端半径判定不可信）。
	const APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}
	const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Npc->GetOwner()->GetActorLocation());
	const float MaxDist = Npc->InteractRadius * 1.5f;
	if (DistSq > MaxDist * MaxDist)
	{
		UE_LOG(LogMADialogue, Warning, TEXT("StartSession 距离验证失败：%s -> %s"),
			*PC->GetName(), *Npc->GetOwner()->GetName());
		return;
	}

	FSession& Session = Sessions.FindOrAdd(PC);
	InterruptActiveRequest(Session);
	Session.bWindowClosed = false; // 可能复用"关窗等收尾"的残留会话
	Session.Npc = Npc;
	Session.History.Reset();
	Session.History.Emplace(EMALLMRole::System, BuildSystemPrompt(*Npc));
	// 开场白是固定台词，也要进上下文 —— 否则模型不知道自己刚说过什么。
	// 按任务状态选词，与客户端 OpenFor 展示的那句一致（同一纯函数计算）。
	FString Greeting = Npc->Greeting;
	if (AMAPlayerState* PS = PC->GetPlayerState<AMAPlayerState>())
	{
		Greeting = MAQuestRules::MakeReturnGreeting(PS->GetActiveQuest(), Npc->Greeting);
	}
	Session.History.Emplace(EMALLMRole::Assistant, MoveTemp(Greeting));
	Session.PendingDeltas.Reset();
	Session.StreamedSoFar.Reset();

	UE_LOG(LogMADialogue, Log, TEXT("会话开始：%s <-> NPC「%s」"), *PC->GetName(), *Npc->NpcName);
}

void UMADialogueSubsystem::SendPlayerMessage(AMAPlayerController* PC, const FString& Text)
{
	FSession* Session = FindSession(PC);
	if (!Session)
	{
		if (PC)
		{
			PC->Client_DialogueError(TEXT("对话尚未开始，请先与NPC交谈"));
		}
		return;
	}

	FString Clean = Text.TrimStartAndEnd().Left(GMaxPlayerMessageLen);
	if (Clean.IsEmpty())
	{
		return;
	}

	// 上一条还在生成 → 打断，已流出的部分保进历史。
	InterruptActiveRequest(*Session);

	Session->History.Emplace(EMALLMRole::User, MoveTemp(Clean));
	TrimHistory(*Session);

	// 任务状态每轮都会变 —— 每次请求前重刷 system prompt 里的任务状态行，
	// 模型才知道当前能不能发任务（发过的会被校验管线拒，但先让它别乱试）。
	if (const UMADialogueComponent* Npc = Session->Npc.Get())
	{
		FString SystemPrompt = BuildSystemPrompt(*Npc);
		if (UMANarrativeSubsystem* Narrative = GetWorld()->GetSubsystem<UMANarrativeSubsystem>())
		{
			SystemPrompt += FString::Printf(TEXT("\n当前玩家任务状态：%s。"
				"答应给玩家任务时必须调用 give_quest 工具正式发布，不得只在口头承诺。"),
				*Narrative->DescribeQuestState(PC));
		}
		Session->History[0].Content = MoveTemp(SystemPrompt);
	}

	const int32 Id = ++Session->MessageId;
	Session->PendingDeltas.Reset();
	Session->StreamedSoFar.Reset();
	Session->FlushAccum = 0.f;

	TWeakObjectPtr<UMADialogueSubsystem> WeakThis(this);
	TWeakObjectPtr<AMAPlayerController> WeakPC(PC);

	// 回调全部在游戏线程（客户端保证），但必须假设 PC/自身可能已死亡。
	FMALLMStreamRequest::FCallbacks Callbacks;
	Callbacks.OnDelta = [WeakThis, WeakPC, Id](const FString& Delta)
	{
		UMADialogueSubsystem* Self = WeakThis.Get();
		AMAPlayerController* Controller = WeakPC.Get();
		if (!Self || !Controller)
		{
			return;
		}
		if (FSession* S = Self->FindSession(Controller); S && S->MessageId == Id)
		{
			S->PendingDeltas += Delta;
			S->StreamedSoFar += Delta;
			if (S->PendingDeltas.Len() >= GFlushImmediatelyAt)
			{
				Self->FlushDeltas(Controller, *S);
			}
		}
	};
	Callbacks.OnComplete = [WeakThis, WeakPC, Id](const FString& FullText, const TArray<FMALLMToolCall>& ToolCalls)
	{
		UMADialogueSubsystem* Self = WeakThis.Get();
		AMAPlayerController* Controller = WeakPC.Get();
		if (!Self || !Controller)
		{
			return;
		}
		FSession* S = Self->FindSession(Controller);
		if (!S || S->MessageId != Id)
		{
			return;
		}
		Self->FlushDeltas(Controller, *S);

		FMALLMMessage AssistantMsg(EMALLMRole::Assistant, FullText);
		AssistantMsg.ToolCalls = ToolCalls;
		S->History.Add(MoveTemp(AssistantMsg));

		// 工具调用轮：逐个过叙事校验管线执行，结果以 tool 消息回填历史 ——
		// 拒绝原因也回填，模型在后续台词里自然圆场。
		FString SpokenFallback;
		if (ToolCalls.Num() > 0)
		{
			UMANarrativeSubsystem* Narrative = Self->GetWorld()
				? Self->GetWorld()->GetSubsystem<UMANarrativeSubsystem>() : nullptr;
			for (const FMALLMToolCall& Call : ToolCalls)
			{
				FString SpokenLine;
				FMALLMMessage ToolMsg(EMALLMRole::Tool, Narrative
					? Narrative->ExecuteToolCall(Controller, S->Npc.Get(), Call, SpokenLine)
					: TEXT("失败：叙事系统不可用。"));
				ToolMsg.ToolCallId = Call.Id;
				S->History.Add(MoveTemp(ToolMsg));
				if (!SpokenLine.IsEmpty())
				{
					SpokenFallback = SpokenLine;
				}
			}
		}

		// 工具调用轮 content 为空（k2.6 实测）—— 用 quest_line 当台词整段推给客户端，
		// 并补进历史，下一轮模型才知道自己说过这句话。
		if (FullText.IsEmpty() && !SpokenFallback.IsEmpty())
		{
			S->PendingDeltas += SpokenFallback;
			Self->FlushDeltas(Controller, *S);
			S->History.Emplace(EMALLMRole::Assistant, SpokenFallback);
		}

		S->ActiveRequest.Reset();
		S->StreamedSoFar.Reset();
		Controller->Client_DialogueCompleted(Id);

		// 关窗等收尾的会话：工具已执行（可能刚注册了待启动任务），
		// 此刻窗口已经是关的 —— 立即补一次关窗通知，然后就地销毁。
		if (S->bWindowClosed)
		{
			if (UMANarrativeSubsystem* Narrative = Self->GetWorld()
				? Self->GetWorld()->GetSubsystem<UMANarrativeSubsystem>() : nullptr)
			{
				Narrative->NotifyDialogueClosed(Controller);
			}
			Self->Sessions.Remove(Controller);
		}
	};
	Callbacks.OnError = [WeakThis, WeakPC, Id](const FString& Message)
	{
		UMADialogueSubsystem* Self = WeakThis.Get();
		AMAPlayerController* Controller = WeakPC.Get();
		if (!Self || !Controller)
		{
			return;
		}
		FSession* S = Self->FindSession(Controller);
		if (!S || S->MessageId != Id)
		{
			return;
		}
		Self->FlushDeltas(Controller, *S);
		if (!S->StreamedSoFar.IsEmpty())
		{
			S->History.Emplace(EMALLMRole::Assistant, S->StreamedSoFar);
			S->StreamedSoFar.Reset();
		}
		S->ActiveRequest.Reset();
		Controller->Client_DialogueError(Message);
		if (S->bWindowClosed)
		{
			Self->Sessions.Remove(Controller);
		}
	};

	// 声明叙事工具 —— 模型可在对话中发起任务/世界事件，执行前都过服务器校验管线。
	FMALLMStreamRequest::FOverrides Overrides;
	Overrides.Tools.Add(UMANarrativeSubsystem::GetGiveQuestToolSpec());
	Overrides.Tools.Add(UMANarrativeSubsystem::GetTriggerRaidToolSpec());
	Overrides.Tools.Add(UMANarrativeSubsystem::GetGrantBlessingToolSpec());

	FString StartError;
	Session->ActiveRequest = FMALLMStreamRequest::Start(Session->History, MoveTemp(Callbacks), StartError, Overrides);
	if (!Session->ActiveRequest.IsValid())
	{
		PC->Client_DialogueError(StartError);
	}
}

bool UMADialogueSubsystem::IsInDialogue(const AMAPlayerController* PC) const
{
	// 关窗等请求收尾的残留会话不算"对话中" —— AI 目标豁免不该继续生效。
	const FSession* S = PC
		? Sessions.Find(TWeakObjectPtr<AMAPlayerController>(const_cast<AMAPlayerController*>(PC))) : nullptr;
	return S && !S->bWindowClosed;
}

void UMADialogueSubsystem::EndSession(AMAPlayerController* PC)
{
	if (const TWeakObjectPtr<AMAPlayerController> Key(PC); Sessions.Contains(Key))
	{
		FSession& Session = Sessions[Key];
		// 关窗时刻通知叙事系统 —— 待启动的任务从这里起跑（剑客离场/刷怪/倒计时）。
		if (UMANarrativeSubsystem* Narrative = GetWorld()->GetSubsystem<UMANarrativeSubsystem>())
		{
			Narrative->NotifyDialogueClosed(PC);
		}
		// 关窗不打断飞行中的请求 —— 工具调用（发任务）必须执行完；
		// 迟到的台词增量客户端会按 stale MessageId 丢弃，无副作用。
		if (Session.ActiveRequest.IsValid() && Session.ActiveRequest->IsActive())
		{
			Session.bWindowClosed = true;
			UE_LOG(LogMADialogue, Log, TEXT("会话关窗但请求飞行中，等它完成：%s"), *PC->GetName());
			return;
		}
		InterruptActiveRequest(Session);
		Sessions.Remove(Key);
		UE_LOG(LogMADialogue, Log, TEXT("会话结束：%s"), *PC->GetName());
	}
}

void UMADialogueSubsystem::Tick(float DeltaTime)
{
	if (Sessions.Num() == 0)
	{
		return;
	}

	const float FlushInterval = UMALLMSettings::Get()->DeltaFlushIntervalSeconds;

	for (auto It = Sessions.CreateIterator(); It; ++It)
	{
		AMAPlayerController* PC = It.Key().Get();
		if (!PC)
		{
			// 玩家断线：取消飞行中的请求，会话随之销毁。
			InterruptActiveRequest(It.Value());
			It.RemoveCurrent();
			continue;
		}

		FSession& Session = It.Value();
		Session.FlushAccum += DeltaTime;
		if (!Session.PendingDeltas.IsEmpty() && Session.FlushAccum >= FlushInterval)
		{
			FlushDeltas(PC, Session);
		}
	}
}

TStatId UMADialogueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMADialogueSubsystem, STATGROUP_Tickables);
}

void UMADialogueSubsystem::Deinitialize()
{
	for (auto& Pair : Sessions)
	{
		InterruptActiveRequest(Pair.Value);
	}
	Sessions.Empty();
	Super::Deinitialize();
}

UMADialogueSubsystem::FSession* UMADialogueSubsystem::FindSession(AMAPlayerController* PC)
{
	return PC ? Sessions.Find(TWeakObjectPtr<AMAPlayerController>(PC)) : nullptr;
}

void UMADialogueSubsystem::FlushDeltas(AMAPlayerController* PC, FSession& Session)
{
	Session.FlushAccum = 0.f;
	if (Session.PendingDeltas.IsEmpty())
	{
		return;
	}
	PC->Client_DialogueDelta(Session.MessageId, Session.PendingDeltas);
	Session.PendingDeltas.Reset();
}

void UMADialogueSubsystem::InterruptActiveRequest(FSession& Session)
{
	if (Session.ActiveRequest.IsValid())
	{
		Session.ActiveRequest->Cancel();
		Session.ActiveRequest.Reset();
	}
	if (!Session.StreamedSoFar.IsEmpty())
	{
		Session.History.Emplace(EMALLMRole::Assistant, Session.StreamedSoFar);
		Session.StreamedSoFar.Reset();
	}
	Session.PendingDeltas.Reset();
}

void UMADialogueSubsystem::TrimHistory(FSession& Session)
{
	const int32 MaxMessages = UMALLMSettings::Get()->MaxHistoryMessages;
	// History[0] 是 system，永远保留；超限时从最旧的非 system 消息开始丢。
	// 带 tool_calls 的 assistant 必须连同其后的 tool 结果一起丢 ——
	// 历史里出现孤儿 tool 消息会被 API 整个请求拒收（400）。
	while (Session.History.Num() > MaxMessages + 1)
	{
		int32 RemoveCount = 1;
		if (Session.History[1].Role == EMALLMRole::Assistant && Session.History[1].ToolCalls.Num() > 0)
		{
			while (Session.History.IsValidIndex(1 + RemoveCount)
				&& Session.History[1 + RemoveCount].Role == EMALLMRole::Tool)
			{
				++RemoveCount;
			}
		}
		Session.History.RemoveAt(1, RemoveCount);
	}
}
