#include "AbilitySystem/AnimNotifies/AN_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

#if WITH_EDITOR
FString UAN_SendGameplayEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid()
		? FString::Printf(TEXT("Event: %s"), *EventTag.ToString())
		: TEXT("Send Gameplay Event (unset)");
}
#endif
