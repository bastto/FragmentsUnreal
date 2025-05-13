

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Index/index_generated.h"
#include "Utils/FragmentsUtils.h"

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

	UFragmentsImporter() {}

	FString Process(AActor* OwnerA, const FString& FragPath, TArray<AFragment*>& OutFragments);
	void SetOwnerRef(AActor* NewOwnerRef) { OwnerRef = NewOwnerRef; }
	//TMap<FString, FSpatialStructure> GetSpatialStructures() { return SpatialStructureData; }
	void GetItemData(AFragment*& InFragment);
	TArray<FItemAttribute> GetItemPropertySets(AFragment* InFragment);
	AFragment* GetItemByLocalId(int32 LocalId, const FString& ModelGuid);

private:

	void CollectPropertiesRecursive(const Model* InModel, int32 StartLocalId, TSet<int32>& Visited, TArray<FItemAttribute>& OutAttributes);
	void SpawnStaticMesh(UStaticMesh* StaticMesh, const Transform* LocalTransform, const Transform* GlobalTransform, AActor* Owner, FName OptionalTag = FName());
	void SpawnFragmentModel(AFragment* InFragmentModel, AActor* InParent, const Meshes* MeshesRef);
	UStaticMesh* CreateStaticMeshFromShell(
		const Shell* ShellRef,
		const Material* RefMaterial,
		const FString& AssetName,
		const FString& AssetPath
	);
	UStaticMesh* CreateStaticMeshFromCircleExtrusion(
		const CircleExtrusion* CircleExtrusion,
		const Material* RefMaterial,
		const FString& AssetName,
		const FString& AssetPath
	);

	FName AddMaterialToMesh(UStaticMesh*& CreatedMesh, const Material* RefMaterial);

	bool TriangulatePolygonWithHoles(const TArray<FVector>& Points,
		const TArray<int32>& Profiles,
		const TArray<TArray<int32>>& Holes,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutIndices);

	void BuildFullCircleExtrusion(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, UStaticMesh* StaticMesh);

	void BuildLineOnlyMesh(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion);

	TArray<FVector> SampleRingPoints(const FVector& Center, const FVector XDir, const FVector& YDir, float Radius, int SegmentCount, float ApertureRadians);

	// variables

private:

	UPROPERTY()
	AActor* OwnerRef = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMaterial = nullptr;
	
	UPROPERTY()
	UMaterialInterface* BaseGlassMaterial = nullptr;

	/*UPROPERTY()
	TMap<FString, FSpatialStructure> SpatialStructureData;*/
	UPROPERTY()
	TMap<FString, class UFragmentModelWrapper*> FragmentModels;
	
	UPROPERTY()
	TMap<FString, FFragmentLookup> ModelFragmentsMap;

public:

	TArray<class AFragment*> FragmentActors;

};
