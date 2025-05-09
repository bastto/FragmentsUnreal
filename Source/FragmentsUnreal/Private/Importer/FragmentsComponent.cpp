


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

void UFragmentsComponent::TestImportFragmentFile(const FString& Path)
{
	if (FragmentsImporter)
		FragmentsImporter->Process(GetOwner(), Path);
}

TArray<class AFragment*> UFragmentsComponent::GetFragmentActors()
{
	if (FragmentsImporter)
		return FragmentsImporter->FragmentActors;

	return TArray<class AFragment*>();
}


