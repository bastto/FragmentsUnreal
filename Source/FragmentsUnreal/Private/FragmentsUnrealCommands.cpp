// Copyright Epic Games, Inc. All Rights Reserved.

#include "FragmentsUnrealCommands.h"

#define LOCTEXT_NAMESPACE "FFragmentsUnrealModule"

void FFragmentsUnrealCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "FragmentsUnreal", "Bring up FragmentsUnreal window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
