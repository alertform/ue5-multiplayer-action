#include "Dialogue/MADialogueSubsystem.h"

#include "Dialogue/MADialogueComponent.h"
#include "LLM/MALLMSettings.h"
#include "LLM/MALLMStreamingClient.h"
#include "Player/MAPlayerController.h"
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
	Session.Npc = Npc;
	Session.History.Reset();
	Session.History.Emplace(EMALLMRole::System, BuildSystemPrompt(*Npc));
	// 开场白是 NPC 的固定台词，也要进上下文 —— 否则模型不知道自己刚说过什么。
	Session.History.Emplace(EMALLMRole::Assistant, Npc->Greeting);
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
	Callbacks.OnComplete = [WeakThis, WeakPC, Id](const FString& FullText)
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
		S->History.Emplace(EMALLMRole::Assistant, FullText);
		S->ActiveRequest.Reset();
		S->StreamedSoFar.Reset();
		Controller->Client_DialogueCompleted(Id);
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
	};

	FString StartError;
	Session->ActiveRequest = FMALLMStreamRequest::Start(Session->History, MoveTemp(Callbacks), StartError);
	if (!Session->ActiveRequest.IsValid())
	{
		PC->Client_DialogueError(StartError);
	}
}

void UMADialogueSubsystem::EndSession(AMAPlayerController* PC)
{
	if (const TWeakObjectPtr<AMAPlayerController> Key(PC); Sessions.Contains(Key))
	{
		InterruptActiveRequest(Sessions[Key]);
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
	while (Session.History.Num() > MaxMessages + 1)
	{
		Session.History.RemoveAt(1);
	}
}
