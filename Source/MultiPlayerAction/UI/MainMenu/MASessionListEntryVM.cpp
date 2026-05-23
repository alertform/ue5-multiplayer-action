#include "UI/MainMenu/MASessionListEntryVM.h"
#include "UI/MainMenu/MAMainMenuViewModel.h"
#include "Online/MASessionSubsystem.h"

void UMASessionListEntryVM::InitFromInfo(const FMASessionInfo& Info, int32 InSourceIndex, UMAMainMenuViewModel* InOwner)
{
	SourceIndex = InSourceIndex;
	Owner = InOwner;

	SetHostName(FText::FromString(Info.OwningUserName));

	const int32 Occupied = Info.NumPublicConnections - Info.NumOpenConnections;
	SetSlotsText(FText::FromString(FString::Printf(TEXT("%d/%d"), Occupied, Info.NumPublicConnections)));

	SetPingText(FText::FromString(FString::Printf(TEXT("%d ms"), Info.PingMs)));
}

void UMASessionListEntryVM::Join()
{
	if (Owner.IsValid())
	{
		Owner->Join(SourceIndex);
	}
}
