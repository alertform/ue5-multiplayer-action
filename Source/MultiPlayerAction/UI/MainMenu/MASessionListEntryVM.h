#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "MASessionListEntryVM.generated.h"

struct FMASessionInfo;
class UMAMainMenuViewModel;

/** One discovered session, shown as a ListView row. Bound to WBP_SessionRow via MVVM. */
UCLASS(BlueprintType)
class MULTIPLAYERACTION_API UMASessionListEntryVM : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void InitFromInfo(const FMASessionInfo& Info, int32 InSourceIndex, UMAMainMenuViewModel* InOwner);

	/** Bound to the row's Join button; routes back to the owning menu VM. */
	UFUNCTION(BlueprintCallable, Category = "Session")
	void Join();

	FText GetHostName() const { return HostName; }
	void SetHostName(const FText& In) { UE_MVVM_SET_PROPERTY_VALUE(HostName, In); }
	FText GetSlotsText() const { return SlotsText; }
	void SetSlotsText(const FText& In) { UE_MVVM_SET_PROPERTY_VALUE(SlotsText, In); }
	FText GetPingText() const { return PingText; }
	void SetPingText(const FText& In) { UE_MVVM_SET_PROPERTY_VALUE(PingText, In); }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = GetHostName, Setter = SetHostName, meta = (AllowPrivateAccess = "true"))
	FText HostName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = GetSlotsText, Setter = SetSlotsText, meta = (AllowPrivateAccess = "true"))
	FText SlotsText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = GetPingText, Setter = SetPingText, meta = (AllowPrivateAccess = "true"))
	FText PingText;

	int32 SourceIndex = INDEX_NONE;
	TWeakObjectPtr<UMAMainMenuViewModel> Owner;
};
