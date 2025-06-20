


#include "Fragment/Fragment.h"
#include <Importer/FragmentsImporterSubsystem.h>


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

TArray<struct FItemAttribute> AFragment::GetAttributes()
{
	if (UWorld* W = GEngine->GetWorldFromContextObjectChecked(this))
	{
		if (auto* Sub = W->GetGameInstance()->GetSubsystem<UFragmentsImporterSubsystem>())
		{
			FFragmentItem* InItem = Sub->GetFragmentItemByLocalId(LocalId, ModelGuid);

			if (!InItem->ModelGuid.IsEmpty())
			{
				Sub->GetItemData(InItem);
				return InItem->Attributes;
			}
		}
	}
	return TArray<FItemAttribute>();
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

void AFragment::SetData(FFragmentItem InFragmentItem)
{
	Data = InFragmentItem;
	ModelGuid = InFragmentItem.ModelGuid;
	Guid = InFragmentItem.Guid;
	GlobalTransform = InFragmentItem.GlobalTransform;
	SetActorTransform(GlobalTransform);
	LocalId = InFragmentItem.LocalId;
	Category = InFragmentItem.Category;
	Samples = InFragmentItem.Samples;
}


