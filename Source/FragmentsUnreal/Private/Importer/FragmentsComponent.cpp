


#include "Importer/FragmentsComponent.h"
#include "Importer/FragmentsImporter.h"
#include "Interfaces/IPluginManager.h"


// Sets default values for this component's properties
UFragmentsComponent::UFragmentsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UFragmentsComponent::BeginPlay()
{
	Super::BeginPlay();
	FragmentsImporter = NewObject<UFragmentsImporter>(this);

	// ...
	
}


// Called every frame
void UFragmentsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

FString UFragmentsComponent::TestImportFragmentFile(const FString& Path, TArray<AFragment*>& OutFragments, bool bSaveMeshes)
{
	if (FragmentsImporter)
	{
#if PLATFORM_ANDROID
		const FString DownloadDir = TEXT("/storage/emulated/0/Download");
		const FString AndroidFilePath = FPaths::Combine(DownloadDir, TEXT("small_test.frag"));

		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*AndroidFilePath))
		{
			FString FileContents;
			if (FFileHelper::LoadFileToString(FileContents, *AndroidFilePath))
			{
				UE_LOG(LogTemp, Log, TEXT("Loaded %s from Download"), *AndroidFilePath);
				return FragmentsImporter->Process(GetOwner(), AndroidFilePath, OutFragments, bSaveMeshes);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to read %s"), *AndroidFilePath);
				// fall through to plugin‐Content fallback
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("File not found in Download: %s"), *AndroidFilePath);
			// fall through to plugin‐Content fallback
		}

#elif PLATFORM_WINDOWS
		// Find our plugin by name
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FragmentsUnreal"));
		if (!Plugin.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Could not find FragmentsUnreal plugin!"));
			return FString();
		}

		// GetContentDir() will point to
		// <Project>/Plugins/FragmentsUnreal/Content  (or Engine/Plugins/..., as appropriate)
		const FString PluginContentDir = Plugin->GetContentDir();

		// Build the full path to our file
		const FString FilePath = FPaths::Combine(PluginContentDir, TEXT("Resources/small_test.frag"));
		
		return FragmentsImporter->Process(GetOwner(), FilePath, OutFragments, bSaveMeshes);

#endif
	}
	return FString();
}

FString UFragmentsComponent::ProcessFragment(const FString& Path, TArray<AFragment*>& OutFragments, bool bSaveMeshes)
{
	if (FragmentsImporter)
		return FragmentsImporter->Process(GetOwner(), Path, OutFragments, bSaveMeshes);

	return FString();
}

TArray<class AFragment*> UFragmentsComponent::GetFragmentActors()
{
	if (FragmentsImporter)
		return FragmentsImporter->FragmentActors;

	return TArray<class AFragment*>();
}

TArray<FItemAttribute> UFragmentsComponent::GetItemPropertySets(AFragment* InFragment)
{
	if (FragmentsImporter)
		return FragmentsImporter->GetItemPropertySets(InFragment);
	return TArray<FItemAttribute>();
}

AFragment* UFragmentsComponent::GetItemByLocalId(int32 LocalId, const FString& ModelGuid)
{
	if (FragmentsImporter) return FragmentsImporter->GetItemByLocalId(LocalId, ModelGuid);
	return nullptr;
}


