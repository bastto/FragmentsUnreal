


#include "Importer/FragmentsComponent.h"
#include "Importer/FragmentsImporter.h"


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


