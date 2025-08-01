

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Index/index_generated.h"
#include "Utils/FragmentsUtils.h"
#include "Importer/DeferredPackageSaveManager.h"
#include "UDynamicMesh.h"

#include "FragmentsImporter.generated.h"


FRAGMENTSUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogFragments, Log, All);
/**
 * 
 */
UCLASS()
class FRAGMENTSUNREAL_API UFragmentsImporter :public UObject
{
	GENERATED_BODY()

public:

	UFragmentsImporter();

	FString Process(AActor* OwnerA, const FString& FragPath, TArray<AFragment*>& OutFragments, bool bSaveMeshes = true, bool bUseDynamicMesh = false);
	void SetOwnerRef(AActor* NewOwnerRef) { OwnerRef = NewOwnerRef; }
	void GetItemData(AFragment*& InFragment);
	void GetItemData(FFragmentItem* InFragmentItem);
	TArray<FItemAttribute> GetItemPropertySets(AFragment* InFragment);
	AFragment* GetItemByLocalId(int32 LocalId, const FString& ModelGuid);
	FFragmentItem* GetFragmentItemByLocalId(int32 LocalId, const FString& InModelGuid);
	FString LoadFragment(const FString& FragPath);
	void ProcessLoadedFragment(const FString& ModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh);
	void ProcessLoadedFragmentItem(int32 InLocalId, const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh);
	TArray<int32> GetElementsByCategory(const FString& InCategory, const FString& ModelGuid);
	void UnloadFragment(const FString& ModelGuid);
	AFragment* GetModelFragment(const FString& ModelGuid);
	FTransform GetBaseCoordinates() { return BaseCoordinates; }

	FORCEINLINE const TMap<FString, class UFragmentModelWrapper*>& GetFragmentModels() const
	{
		return FragmentModels;
	}

private:

	void CollectPropertiesRecursive(const Model* InModel, int32 StartLocalId, TSet<int32>& Visited, TArray<FItemAttribute>& OutAttributes);
	void SpawnStaticMesh(UStaticMesh* StaticMesh, const Transform* LocalTransform, const Transform* GlobalTransform, AActor* Owner, FName OptionalTag = FName());
	void SpawnFragmentModel(AFragment* InFragmentModel, AActor* InParent, const Meshes* MeshesRef, bool bSaveMeshes);
	AFragment* SpawnFragmentModel(FFragmentItem InFragmentItem, AActor* InParent, const Meshes* MeshesRef, bool bSaveMeshes, class UFragmentModelWrapper* InWrapperRef, bool bUseDynamicMesh);
	UStaticMesh* CreateStaticMeshFromShell(
		const Shell* ShellRef,
		const Material* RefMaterial,
		const FString& AssetName,
		UObject* OuterRef
	);
	UStaticMesh* CreateStaticMeshFromCircleExtrusion(
		const CircleExtrusion* CircleExtrusion,
		const Material* RefMaterial,
		const FString& AssetName,
		UObject* OuterRef
	);
	FDynamicMesh3 CreateDynamicMeshFromShell(
		const Shell* ShellRef,
		const Material* RefMaterial,
		const FString& AssetName,
		UObject* OuterRef
	);
	FDynamicMesh3 CreateDynamicMeshFromCircleExtrusion(
		const CircleExtrusion* CircleExtrusion,
		const Material* RefMaterial,
		const FString& AssetName,
		UObject* OuterRef
	);

	FName AddMaterialToMesh(UStaticMesh*& CreatedMesh, const Material* RefMaterial);
	void AddMaterialToDynamicMesh(class UDynamicMeshComponent* InDynComp, const Material* RefMaterial, UFragmentModelWrapper* InWrapperRef, int32 InMaterialIndex);

	bool TriangulatePolygonWithHoles(const TArray<FVector>& Points,
		const TArray<int32>& Profiles,
		const TArray<TArray<int32>>& Holes,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutIndices);

	void BuildFullCircleExtrusion(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, UStaticMesh* StaticMesh);

	void BuildLineOnlyMesh(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion);

	TArray<FVector> SampleRingPoints(const FVector& Center, const FVector XDir, const FVector& YDir, float Radius, int SegmentCount, float ApertureRadians);

	void SavePackagesWithProgress(const TArray<UPackage*>& InPackagesToSave);

	// variables

private:

	UPROPERTY()
	AActor* OwnerRef = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMaterial = nullptr;
	
	UPROPERTY()
	UMaterialInterface* BaseGlassMaterial = nullptr;

	UPROPERTY()
	TMap<FString, class UFragmentModelWrapper*> FragmentModels;

	UPROPERTY()
	TMap<FString, FFragmentLookup> ModelFragmentsMap;

	UPROPERTY()
	TMap<FString, UStaticMesh*> MeshCache;

	UPROPERTY()
	TArray<UPackage*> PackagesToSave;

	UPROPERTY()
	bool bBaseCoordinatesInitialized = false;

	UPROPERTY()
	FTransform BaseCoordinates;

	FDeferredPackageSaveManager DeferredSaveManager;

public:

	TArray<class AFragment*> FragmentActors;

};
