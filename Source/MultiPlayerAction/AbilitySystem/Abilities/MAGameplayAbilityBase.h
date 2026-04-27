#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MAGameplayAbilityBase.generated.h"

/**
 * Base class for all gameplay abilities in MultiPlayerAction.
 * Provides helper accessors for ASC, Avatar, and AttributeSet.
 */
UCLASS(Abstract)
class MULTIPLAYERACTION_API UMAGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMAGameplayAbilityBase();

	/** Input tag used to bind this ability to Enhanced Input */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag InputTag;

protected:
	/** Get the avatar actor cast to our character class */
	class AMultiPlayerActionCharacter* GetMACharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
};
