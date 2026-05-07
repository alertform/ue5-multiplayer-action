#include "Player/MAPlayerController.h"
#include "Player/MAPlayerState.h"
#include "UI/MAUserWidget.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "UObject/ConstructorHelpers.h"

AMAPlayerController::AMAPlayerController()
{
	// Bind WBP_HUD asset directly here so the C++ class is self-sufficient
	// (no BP_MAPlayerController child needed; matches the BP_ThirdPersonCharacter
	// ClassFinder pattern in MultiPlayerActionGameMode.cpp)
	static ConstructorHelpers::FClassFinder<UMAUserWidget> HUDWidgetBPClass(TEXT("/Game/UI/WBP_HUD"));
	if (HUDWidgetBPClass.Succeeded())
	{
		HUDWidgetClass = HUDWidgetBPClass.Class;
	}
}

void AMAPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureHUDInitialized();
}

void AMAPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	// Client path: PS just replicated — retry HUD init in case BeginPlay ran first
	EnsureHUDInitialized();
}

void AMAPlayerController::DamageSelf(float Amount)
{
	AMAPlayerState* PS = GetPlayerState<AMAPlayerState>();
	if (!PS) return;
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC) return;

	const float Cur = ASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute());
	ASC->SetNumericAttributeBase(UMAAttributeSet::GetHealthAttribute(), FMath::Max(0.f, Cur - Amount));
}

void AMAPlayerController::EnsureHUDInitialized()
{
	// Skip on dedicated server / remote clients
	if (!IsLocalController() || !HUDWidgetClass)
	{
		return;
	}

	if (!HUDWidget)
	{
		HUDWidget = CreateWidget<UMAUserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}

	// Bind to ASC if PS replicated. Idempotent on the widget side.
	if (HUDWidget)
	{
		if (AMAPlayerState* PS = GetPlayerState<AMAPlayerState>())
		{
			HUDWidget->InitFromASC(PS->GetAbilitySystemComponent());
		}
	}
}
