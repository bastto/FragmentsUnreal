


#include "Importer/FragmentsImporterSubsystem.h"
#include "Importer/FragmentsImporter.h"


FString UFragmentsImporterSubsystem::LoadFragment(const FString& FragPath)
{
    check(Importer);
    return Importer->LoadFragment(FragPath);
}

void UFragmentsImporterSubsystem::UnloadFragment(const FString& ModelGuid)
{
    check(Importer);
    Importer->UnloadFragment(ModelGuid);
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
