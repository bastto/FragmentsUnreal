


#include "EditorSubsystems/FragmentsImporterEditorSubsystem.h"
#include "Importer/FragmentsImporter.h"

FString UFragmentsImporterEditorSubsystem::LoadFragment(const FString& FragPath)
{
    check(Importer);

    FString ModelGuid = Importer->LoadFragment(FragPath);
    FragmentModels = Importer->GetFragmentModels();

    return ModelGuid;
}

void UFragmentsImporterEditorSubsystem::UnloadFragment(const FString& ModelGuid)
{
    if (Importer)
        Importer->UnloadFragment(ModelGuid);

    if (UFragmentModelWrapper** Found = FragmentModels.Find(ModelGuid))
    {
        (*Found) = nullptr;
        FragmentModels.Remove(ModelGuid);
    }
}

FString UFragmentsImporterEditorSubsystem::ProcessFragment(AActor* OwnerActor, const FString& FragPath, TArray<class AFragment*>& OutFragments, bool bSaveMeshes, bool bUseDynamicMesh)
{
    check(Importer);

    FString ModelGuid = Importer->Process(OwnerActor, FragPath, OutFragments, bSaveMeshes, bUseDynamicMesh);

    FragmentModels = Importer->GetFragmentModels();

    return ModelGuid;
}

void UFragmentsImporterEditorSubsystem::ProcessLoadedFragment(const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh)
{
    check(Importer);

    Importer->ProcessLoadedFragment(InModelGuid, InOwnerRef, bInSaveMesh, bUseDynamicMesh);
}

void UFragmentsImporterEditorSubsystem::ProcessLoadedFragmentItem(int32 InLocalId, const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh)
{
    check(Importer);

    return Importer->ProcessLoadedFragmentItem(InLocalId, InModelGuid, InOwnerRef, bInSaveMesh, bUseDynamicMesh);
}

TArray<int32> UFragmentsImporterEditorSubsystem::GetElementsByCategory(const FString& InCategory, const FString& ModelGuid)
{
    check(Importer);

    return Importer->GetElementsByCategory(InCategory, ModelGuid);
}

AFragment* UFragmentsImporterEditorSubsystem::GetItemByLocalId(int32 InLocalId, const FString& InModelGuid)
{
    check(Importer)

        return Importer->GetItemByLocalId(InLocalId, InModelGuid);
}

FFragmentItem* UFragmentsImporterEditorSubsystem::GetFragmentItemByLocalId(int32 InLocalId, const FString& InModelGuid)
{
    check(Importer);
    return Importer->GetFragmentItemByLocalId(InLocalId, InModelGuid);
}

void UFragmentsImporterEditorSubsystem::GetItemData(FFragmentItem* InFragmentItem)
{
    check(Importer);
    Importer->GetItemData(InFragmentItem);
}

AFragment* UFragmentsImporterEditorSubsystem::GetModelFragment(const FString& InModelGuid)
{
    check(Importer);
    return Importer->GetModelFragment(InModelGuid);
}

FTransform UFragmentsImporterEditorSubsystem::GetBaseCoordinates()
{
    check(Importer);
    return Importer->GetBaseCoordinates();
}

void UFragmentsImporterEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Importer = NewObject<UFragmentsImporter>(this);
}

void UFragmentsImporterEditorSubsystem::Deinitialize()
{
    if (Importer)
    {
        Importer = nullptr;
    }

    Super::Deinitialize();
}