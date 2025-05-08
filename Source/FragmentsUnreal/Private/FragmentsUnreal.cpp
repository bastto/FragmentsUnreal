// Copyright Epic Games, Inc. All Rights Reserved.

#include "FragmentsUnreal.h"
//#include "FragmentsUnrealStyle.h"
//#include "FragmentsUnrealCommands.h"
//#include "LevelEditor.h"
//#include "Widgets/Docking/SDockTab.h"
//#include "Widgets/Layout/SBox.h"
//#include "Widgets/Text/STextBlock.h"
//#include "ToolMenus.h"

static const FName FragmentsUnrealTabName("FragmentsUnreal");

#define LOCTEXT_NAMESPACE "FFragmentsUnrealModule"

//void FFragmentsUnrealModule::StartupModule()
//{
//	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
//	
//	FFragmentsUnrealStyle::Initialize();
//	FFragmentsUnrealStyle::ReloadTextures();
//
//	FFragmentsUnrealCommands::Register();
//	
//	PluginCommands = MakeShareable(new FUICommandList);
//
//	PluginCommands->MapAction(
//		FFragmentsUnrealCommands::Get().OpenPluginWindow,
//		FExecuteAction::CreateRaw(this, &FFragmentsUnrealModule::PluginButtonClicked),
//		FCanExecuteAction());
//
//	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FFragmentsUnrealModule::RegisterMenus));
//	
//	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FragmentsUnrealTabName, FOnSpawnTab::CreateRaw(this, &FFragmentsUnrealModule::OnSpawnPluginTab))
//		.SetDisplayName(LOCTEXT("FFragmentsUnrealTabTitle", "FragmentsUnreal"))
//		.SetMenuType(ETabSpawnerMenuType::Hidden);
//}
//
//void FFragmentsUnrealModule::ShutdownModule()
//{
//	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
//	// we call this function before unloading the module.
//
//	UToolMenus::UnRegisterStartupCallback(this);
//
//	UToolMenus::UnregisterOwner(this);
//
//	FFragmentsUnrealStyle::Shutdown();
//
//	FFragmentsUnrealCommands::Unregister();
//
//	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FragmentsUnrealTabName);
//}
//
//TSharedRef<SDockTab> FFragmentsUnrealModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
//{
//	FText WidgetText = FText::Format(
//		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
//		FText::FromString(TEXT("FFragmentsUnrealModule::OnSpawnPluginTab")),
//		FText::FromString(TEXT("FragmentsUnreal.cpp"))
//		);
//
//	return SNew(SDockTab)
//		.TabRole(ETabRole::NomadTab)
//		[
//			// Put your tab content here!
//			SNew(SBox)
//			.HAlign(HAlign_Center)
//			.VAlign(VAlign_Center)
//			[
//				SNew(STextBlock)
//				.Text(WidgetText)
//			]
//		];
//}
//
//void FFragmentsUnrealModule::PluginButtonClicked()
//{
//	FGlobalTabmanager::Get()->TryInvokeTab(FragmentsUnrealTabName);
//}
//
//void FFragmentsUnrealModule::RegisterMenus()
//{
//	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
//	FToolMenuOwnerScoped OwnerScoped(this);
//
//	{
//		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
//		{
//			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
//			Section.AddMenuEntryWithCommandList(FFragmentsUnrealCommands::Get().OpenPluginWindow, PluginCommands);
//		}
//	}
//
//	{
//		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
//		{
//			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
//			{
//				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FFragmentsUnrealCommands::Get().OpenPluginWindow));
//				Entry.SetCommandList(PluginCommands);
//			}
//		}
//	}
//}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFragmentsUnrealModule, FragmentsUnreal)
