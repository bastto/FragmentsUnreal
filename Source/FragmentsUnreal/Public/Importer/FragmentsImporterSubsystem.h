

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FragmentsImporterSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class FRAGMENTSUNREAL_API UFragmentsImporterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	FString LoadFragment(const FString& FragPath);
	void UnloadFragment(const FString& ModelGuid);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	UPROPERTY()
	TMap<FString, class UFragmentModelWrapper*> FragmentModels;

	UPROPERTY()
	class UFragmentsImporter* Importer = nullptr;

};
