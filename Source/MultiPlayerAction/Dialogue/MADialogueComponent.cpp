#include "Dialogue/MADialogueComponent.h"

#include "Components/WidgetComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UI/MANpcNameplateWidget.h"

void UMADialogueComponent::BeginPlay()
{
	Super::BeginPlay();

	// 特效分发需要 GameState 委托，晚到就轮询（绑上即停 tick）。
	PrimaryComponentTick.bCanEverTick = true;
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
		return; // 未配置 = 该节拍静默
	}
	for (const FVector& Location : Locations)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location);
	}
}
