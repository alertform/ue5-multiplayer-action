#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AN_SendGameplayEvent.generated.h"

/**
 * AnimNotify that fires a GameplayEvent on the owning actor's ASC.
 * Place on a montage at the impact frame; pair with WaitGameplayEvent in a GameplayAbility.
 */
UCLASS(meta = (DisplayName = "Send Gameplay Event"))
class MULTIPLAYERACTION_API UAN_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Tag to send (e.g. Event.Montage.Hit). Configure per-notify in the montage timeline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEvent")
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual FString GetNotifyName_Implementation() const override;
#endif
};
