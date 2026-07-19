#include "Dialogue/MADialogueComponent.h"

#include "Components/WidgetComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UI/MANpcNameplateWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogMADialogueFX, Log, All);

UMADialogueComponent::UMADialogueComponent()
{
	// tick 只为轮询绑定 GameState 特效委托（绑上即停）。
	// bCanEverTick 必须在构造期设 —— tick 函数在 RegisterComponent 时注册，
	// BeginPlay 里再设为时已晚（实测特效全哑的根因）。
	PrimaryComponentTick.bCanEverTick = true;
}

void UMADialogueComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(true);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 头顶铭牌：screen-space、按需尺寸，挂根组件上方。DS 上没有本地玩家也无妨 ——
	// widget 的提示行靠 GetOwningPlayerPawn 判距，取不到就保持收起。
	NameplateComponent = NewObject<UWidgetComponent>(Owner, TEXT("NpcNameplate"));
	NameplateComponent->SetupAttachment(Owner->GetRootComponent());
	NameplateComponent->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateComponent->SetDrawAtDesiredSize(true);
	NameplateComponent->SetWidgetClass(UMANpcNameplateWidget::StaticClass());
	NameplateComponent->RegisterComponent();

	if (UMANpcNameplateWidget* Nameplate = Cast<UMANpcNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
	{
		Nameplate->Setup(NpcName, InteractRadius, Owner);
	}
}

void UMADialogueComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bBoundToGameState)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		GS->OnNarrativeFXEvent.AddUObject(this, &UMADialogueComponent::HandleNarrativeFX);
		bBoundToGameState = true;
		SetComponentTickEnabled(false);
	}
}

void UMADialogueComponent::HandleNarrativeFX(EMANarrativeFX Type, const TArray<FVector>& Locations)
{
	// DS 无渲染不播；listen host / 客户端各自本地播。
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UNiagaraSystem* System = nullptr;
	switch (Type)
	{
	case EMANarrativeFX::GiverVanish: System = GiverVanishFX; break;
	case EMANarrativeFX::GiverAppear: System = GiverAppearFX ? GiverAppearFX.Get() : GiverVanishFX.Get(); break;
	case EMANarrativeFX::EnemySpawn:  System = EnemySpawnFX; break;
	}
	if (!System)
	{
		UE_LOG(LogMADialogueFX, Log, TEXT("叙事特效跳过（type=%d 未配置资产）"), static_cast<int32>(Type));
		return; // 未配置 = 该节拍静默
	}
	UE_LOG(LogMADialogueFX, Display, TEXT("叙事特效播放：type=%d %s × %d 落点"),
		static_cast<int32>(Type), *System->GetName(), Locations.Num());
	for (const FVector& Location : Locations)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location);
	}
}
