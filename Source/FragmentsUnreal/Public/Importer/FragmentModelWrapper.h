

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Index/index_generated.h"
#include "Utils/FragmentsUtils.h"
#include "FragmentModelWrapper.generated.h"

/**
 * 
 */
UCLASS()
class FRAGMENTSUNREAL_API UFragmentModelWrapper : public UObject
{
	GENERATED_BODY()

	
private:

	UPROPERTY()
	TArray<uint8> RawBuffer;

	const Model* ParsedModel = nullptr;

	FFragmentItem ModelItem;

	UPROPERTY()
	class AFragment* SpawnedFragment;


public:
	void LoadModel(const TArray<uint8>& InBuffer)
	{
		RawBuffer = InBuffer;
		ParsedModel = GetModel(RawBuffer.GetData());
	}

	const Model* GetParsedModel() { return ParsedModel; }

	void SetModelItem(FFragmentItem InModelItem) { ModelItem = InModelItem; }
	FFragmentItem GetModelItem() { return ModelItem; }
	void SetSpawnedFragment(class AFragment* InSpawnedFragment) { SpawnedFragment = InSpawnedFragment; }
	class AFragment* GetSpawnedFragment() { return SpawnedFragment; }

};
