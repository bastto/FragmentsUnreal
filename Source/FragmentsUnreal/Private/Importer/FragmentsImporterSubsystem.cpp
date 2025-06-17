


#include "Importer/FragmentsImporterSubsystem.h"
#include "Importer/FragmentsImporter.h"
#include "Importer/FragmentModelWrapper.h"


FString UFragmentsImporterSubsystem::LoadFragment(const FString& FragPath)
{
    check(Importer);

    FString ModelGuid = Importer->LoadFragment(FragPath);
    FragmentModels = Importer->GetFragmentModels();

    return ModelGuid;
}

void UFragmentsImporterSubsystem::UnloadFragment(const FString& ModelGuid)
{
    if (Importer)
        Importer->UnloadFragment(ModelGuid);

    if (UFragmentModelWrapper** Found = FragmentModels.Find(ModelGuid))
    {
        (*Found) = nullptr;
        FragmentModels.Remove(ModelGuid);
    }
}

FString UFragmentsImporterSubsystem::ProcessFragment(AActor* OwnerActor, const FString& FragPath, TArray<class AFragment*>& OutFragments, bool bSaveMeshes)
{
    check(Importer);

    FString ModelGuid = Importer->Process(OwnerActor, FragPath, OutFragments, bSaveMeshes);

    FragmentModels = Importer->GetFragmentModels();

    return ModelGuid;
}

void UFragmentsImporterSubsystem::ProcessLoadedFragment(const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh)
{
    check(Importer);

    Importer->ProcessLoadedFragment(InModelGuid, InOwnerRef, bInSaveMesh);
}

TArray<int32> UFragmentsImporterSubsystem::GetElementsByCategory(const FString& InCategory, const FString& ModelGuid)
{
    check(Importer);

    return Importer->GetElementsByCategory(InCategory, ModelGuid);
}

FFragmentItem* UFragmentsImporterSubsystem::GetFragmentItemByLocalId(int32 InLocalId, const FString& InModelGuid)
{
    check(Importer);
    return Importer->GetFragmentItemByLocalId(InLocalId, InModelGuid);
}

void UFragmentsImporterSubsystem::GetItemData(FFragmentItem* InFragmentItem)
{
    check(Importer);
    Importer->GetItemData(InFragmentItem);
}

void UFragmentsImporterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Importer = NewObject<UFragmentsImporter>(this);
}

void UFragmentsImporterSubsystem::Deinitialize()
{
    if (Importer)
    {
        Importer = nullptr;
    }

    Super::Deinitialize();
}
