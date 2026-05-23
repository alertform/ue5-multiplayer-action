#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "MAMainMenuViewModel.generated.h"

class UMASessionSubsystem;
class UMASessionListEntryVM;
class APlayerController;
struct FMASessionInfo;

/** Fired after the Sessions array is rebuilt; the View pushes it into the ListView. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMAOnSessionsChanged);

/** Drives WBP_MainMenu. Owns all menu state + commands; binds the session subsystem delegates. */
UCLASS(BlueprintType)
class MULTIPLAYERACTION_API UMAMainMenuViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Idempotent: caches subsystem + PC, binds subsystem delegates once. */
	void Initialize(UMASessionSubsystem* InSubsystem, APlayerController* InOwningPC);

	UFUNCTION(BlueprintCallable, Category = "Menu") void Host();
	UFUNCTION(BlueprintCallable, Category = "Menu") void Refresh();
	UFUNCTION(BlueprintCallable, Category = "Menu") void Join(int32 EntryIndex);
	UFUNCTION(BlueprintCallable, Category = "Menu") void Quit();

	const TArray<TObjectPtr<UMASessionListEntryVM>>& GetSessions() const { return Sessions; }

	UPROPERTY(BlueprintAssignable, Category = "Menu")
	FMAOnSessionsChanged OnSessionsChanged;

	bool IsBusy() const { return bIsBusy; }
	void SetIsBusy(bool In) { UE_MVVM_SET_PROPERTY_VALUE(bIsBusy, In); }
	FText GetStatusText() const { return StatusText; }
	void SetStatusText(const FText& In) { UE_MVVM_SET_PROPERTY_VALUE(StatusText, In); }
	int32 GetMaxPlayers() const { return MaxPlayers; }
	void SetMaxPlayers(int32 In) { UE_MVVM_SET_PROPERTY_VALUE(MaxPlayers, FMath::Clamp(In, 1, 8)); }
	FString GetPlayerName() const { return PlayerName; }
	void SetPlayerName(const FString& In) { UE_MVVM_SET_PROPERTY_VALUE(PlayerName, In); }
	bool HasSessions() const { return bHasSessions; }
	void SetHasSessions(bool In) { UE_MVVM_SET_PROPERTY_VALUE(bHasSessions, In); }

private:
	UFUNCTION() void HandleHostComplete(bool bSucceeded);
	UFUNCTION() void HandleFindComplete(bool bSucceeded, const TArray<FMASessionInfo>& InSessions);
	UFUNCTION() void HandleJoinComplete(bool bSucceeded);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = IsBusy, Setter = SetIsBusy, meta = (AllowPrivateAccess = "true"))
	bool bIsBusy = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = GetStatusText, Setter = SetStatusText, meta = (AllowPrivateAccess = "true"))
	FText StatusText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = GetMaxPlayers, Setter = SetMaxPlayers, meta = (AllowPrivateAccess = "true"))
	int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter = GetPlayerName, Setter = SetPlayerName, meta = (AllowPrivateAccess = "true"))
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = HasSessions, Setter = SetHasSessions, meta = (AllowPrivateAccess = "true"))
	bool bHasSessions = false;

	UPROPERTY()
	TArray<TObjectPtr<UMASessionListEntryVM>> Sessions;

	UPROPERTY()
	TObjectPtr<UMASessionSubsystem> Subsystem;

	TWeakObjectPtr<APlayerController> OwningPC;
	bool bInitialized = false;
};
