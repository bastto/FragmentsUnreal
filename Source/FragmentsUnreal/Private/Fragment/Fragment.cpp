


#include "Fragment/Fragment.h"


// Sets default values
AFragment::AFragment()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFragment::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFragment::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

AFragment* AFragment::FindFragmentByLocalId(int32 InLocalId)
{
	if (InLocalId == LocalId) return this;

	AFragment* FoundFragment = nullptr;
	for (AFragment*& F : FragmentChildren)
	{
		if (F->GetLocalId() == InLocalId) return F;

		FoundFragment = F->FindFragmentByLocalId(InLocalId);
	}
	return FoundFragment;
}


