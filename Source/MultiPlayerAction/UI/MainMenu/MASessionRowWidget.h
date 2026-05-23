#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MASessionRowWidget.generated.h"

/** C++ base for WBP_SessionRow. Injects the row's UMASessionListEntryVM when the ListView assigns it. */
UCLASS(Abstract)
class MULTIPLAYERACTION_API UMASessionRowWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
