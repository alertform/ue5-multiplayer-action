#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "MATargetDummy.generated.h"

class UMAAbilitySystemComponent;
class UMAAttributeSet;

/**
 * Stationary practice dummy. Owns its own ASC + AttributeSet (no PlayerState),
 * so melee Damage GE can resolve a valid target ASC and we can verify the loop in PIE.
 */
UCLASS()
class MULTIPLAYERACTION_API AMATargetDummy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMATargetDummy();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMAAttributeSet> AttributeSet;
};
