

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Utils/FragmentsUtils.h"
#include "FragmentsImporterEditorSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class FRAGMENTSEDITOR_API UFragmentsImporterEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable)
	FString LoadFragment(const FString& FragPath);

	UFUNCTION(BlueprintCallable)
	void UnloadFragment(const FString& ModelGuid);

	UFUNCTION(BlueprintCallable)
	FString ProcessFragment(AActor* OwnerActor, const FString& FragPath, TArray<class AFragment*>& OutFragments, bool bSaveMeshes, bool bUseDynamicMesh);

	UFUNCTION(BlueprintCallable)
	void ProcessLoadedFragment(const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh);

	UFUNCTION(BlueprintCallable)
	void ProcessLoadedFragmentItem(int32 InLocalId, const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh);

	UFUNCTION(BlueprintCallable)
	TArray<int32> GetElementsByCategory(const FString& InCategory, const FString& ModelGuid);

	UFUNCTION(BlueprintCallable)
	AFragment* GetItemByLocalId(int32 InLocalId, const FString& InModelGuid);

	FFragmentItem* GetFragmentItemByLocalId(int32 InLocalId, const FString& InModelGuid);
	void GetItemData(FFragmentItem* InFragmentItem);

	UFUNCTION(BlueprintCallable)
	AFragment* GetModelFragment(const FString& InModelGuid);

	FORCEINLINE const TMap<FString, class UFragmentModelWrapper*>& GetFragmentModels() const
	{
		return FragmentModels;
	}

protected:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	UPROPERTY()
	TMap<FString, class UFragmentModelWrapper*> FragmentModels;

	UPROPERTY()
	class UFragmentsImporter* Importer = nullptr;

};
