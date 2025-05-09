

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Index/index_generated.h"

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

	void Process(AActor* OwnerA, const FString& FragPath);
	void SetOwnerRef(AActor* NewOwnerRef) { OwnerRef = NewOwnerRef; }

private:

	void SpawnStaticMesh(UStaticMesh* StaticMesh, const Transform* LocalTransform, const Transform* GlobalTransform, AActor* Owner, FName OptionalTag = FName());
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

	// Create projection axes for an arbitrary polygon
	//FPlaneProjection BuildProjectionPlane(const TArray<FVector>& Points, const TArray<int32>& Profile)
	//{
	//	FPlaneProjection Projection;

	//	if (Profile.Num() < 3)
	//		return Projection;

	//	const FVector A = Points[Profile[0]];
	//	FVector B, C;
	//	bool bFound = false;

	//	for (int32 i = 1; i < Profile.Num() - 1; ++i)
	//	{
	//		B = Points[Profile[i]];
	//		C = Points[Profile[i + 1]];

	//		FVector Normal = FVector::CrossProduct(B - A, C - A);
	//		if (!Normal.IsNearlyZero())
	//		{
	//			Normal.Normalize();
	//			FVector AxisX = (B - A).GetSafeNormal();
	//			FVector AxisY = FVector::CrossProduct(Normal, AxisX).GetSafeNormal();

	//			Projection.Origin = A;
	//			Projection.AxisX = AxisX;
	//			Projection.AxisY = AxisY;
	//			bFound = true;
	//			break;
	//		}
	//	}

	//	if (!bFound)
	//	{
	//		// Fallback: default projection
	//		UE_LOG(LogTemp, Error, TEXT("Failed to find non-collinear points in profile! Projection may be invalid."));
	//		Projection.Origin = A;
	//		Projection.AxisX = FVector(1, 0, 0);
	//		Projection.AxisY = FVector(0, 1, 0);
	//	}

	//	return Projection;
	//}

	/*bool IsClockwise(const TArray<FVector2D>& Points)
	{
		float Area = 0.0f;
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FVector2D& p0 = Points[i];
			const FVector2D& p1 = Points[(i + 1) % Points.Num()];
			Area += (p1.X - p0.X) * (p1.Y + p0.Y);
		}
		return Area > 0.0;
	}*/

	UPROPERTY()
	AActor* OwnerRef = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMaterial = nullptr;
	
	UPROPERTY()
	UMaterialInterface* BaseGlassMaterial = nullptr;

	const Model* ModelRef = nullptr;

};
