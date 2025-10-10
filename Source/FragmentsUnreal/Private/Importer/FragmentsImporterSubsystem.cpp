


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

FString UFragmentsImporterSubsystem::ProcessFragment(AActor* OwnerActor, const FString& FragPath, TArray<class AFragment*>& OutFragments, bool bSaveMeshes, bool bUseDynamicMesh)
{
    check(Importer);

    FString ModelGuid = Importer->Process(OwnerActor, FragPath, OutFragments, bSaveMeshes, bUseDynamicMesh);

    FragmentModels = Importer->GetFragmentModels();

    return ModelGuid;
}

void UFragmentsImporterSubsystem::ProcessLoadedFragment(const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh)
{
    check(Importer);

    Importer->ProcessLoadedFragment(InModelGuid, InOwnerRef, bInSaveMesh, bUseDynamicMesh);
}

void UFragmentsImporterSubsystem::ProcessLoadedFragmentItem(int32 InLocalId, const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh)
{
    check(Importer);

    return Importer->ProcessLoadedFragmentItem(InLocalId, InModelGuid, InOwnerRef, bInSaveMesh, bUseDynamicMesh);
}

TArray<int32> UFragmentsImporterSubsystem::GetElementsByCategory(const FString& InCategory, const FString& ModelGuid)
{
    check(Importer);

    return Importer->GetElementsByCategory(InCategory, ModelGuid);
}

AFragment* UFragmentsImporterSubsystem::GetItemByLocalId(int32 InLocalId, const FString& InModelGuid)
{
    check (Importer)

    return Importer->GetItemByLocalId(InLocalId, InModelGuid);
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

TArray<FItemAttribute> UFragmentsImporterSubsystem::GetItemPropertySets(int32 LocalId, const FString& InModelGuid)
{
    check(Importer);
    return Importer->GetItemPropertySets(LocalId, InModelGuid);
}

AFragment* UFragmentsImporterSubsystem::GetModelFragment(const FString& InModelGuid)
{
    check(Importer);
    return Importer->GetModelFragment(InModelGuid);
}

FTransform UFragmentsImporterSubsystem::GetBaseCoordinates()
{
    check(Importer);
    return Importer->GetBaseCoordinates();
}

void UFragmentsImporterSubsystem::ResetBaseCoordinates()
{
    check(Importer);
    Importer->ResetBaseCoordinates();
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
