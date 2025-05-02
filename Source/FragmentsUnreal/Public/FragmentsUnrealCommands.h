// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "FragmentsUnrealStyle.h"

class FFragmentsUnrealCommands : public TCommands<FFragmentsUnrealCommands>
{
public:

	FFragmentsUnrealCommands()
		: TCommands<FFragmentsUnrealCommands>(TEXT("FragmentsUnreal"), NSLOCTEXT("Contexts", "FragmentsUnreal", "FragmentsUnreal Plugin"), NAME_None, FFragmentsUnrealStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};
