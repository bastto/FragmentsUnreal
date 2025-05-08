

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Index/index_generated.h"

#include "FragmentsImporter.generated.h"

struct FFragmentEdge
{
	float A, B;

	FFragmentEdge(float InA, float InB)
	{
		A = InA;
		B = InB;
	}

	bool operator==(const FFragmentEdge& Other) const
	{
		return (A == Other.A && B == Other.B) || (A == Other.B && B == Other.A);
	}
};

struct FPlaneProjection
{
	FVector Origin;
	FVector AxisX;
	FVector AxisY;

	FVector2D Project(const FVector& Point) const
	{
		FVector Local = Point - Origin;
		return FVector2D(FVector::DotProduct(Local, AxisX), FVector::DotProduct(Local, AxisY));
	}

	FVector Unproject(const FVector2D& Point2D) const
	{
		return Origin + AxisX * Point2D.X + AxisY * Point2D.Y;
	}
};



struct FProjectionPlane
{
	FVector Origin;
	TArray<FVector> OriginalPoints;
	FVector U;
	FVector V;

	bool Initialize(const TArray<FVector>& Points)
	{
		if (Points.Num() < 3)
		{
			UE_LOG(LogTemp, Error, TEXT("[Projection] Not enoguht points (%d) to define a plane."), Points.Num());
			return false;
		}

		OriginalPoints = Points;
		Origin = Points[0];

		FVector A, B;
		bool bFound = false;
		for (int32 i = 1; i < Points.Num() - 1; i++)
		{
			A = Points[i] - Origin;
			B = Points[i + 1] - Origin;

			if (!A.IsNearlyZero() && !B.IsNearlyZero() && !A.Equals(B, KINDA_SMALL_NUMBER))
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			UE_LOG(LogTemp, Error, TEXT("[Projection] Could not find non-collinear points."));
			return false;
		}

		FVector Normal = FVector::CrossProduct(A, B).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			UE_LOG(LogTemp, Error, TEXT("[Projection] Cross product resulted in zero normal (points may be collinear)."));
			return false;
		}

		U = A.GetSafeNormal();
		V = FVector::CrossProduct(Normal, U);
		return true;
	}

	FVector2D Project(const FVector& P) const {
		FVector Local = P - Origin;
		return FVector2D(FVector::DotProduct(Local, U), FVector::DotProduct(Local, V));
	}
};

struct FTriangulationResult
{
	TArray<FVector> FlattenedPoints;
	TArray<int32> TriangleIndices;
};

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
	FPlaneProjection BuildProjectionPlane(const TArray<FVector>& Points, const TArray<int32>& Profile)
	{
		FPlaneProjection Projection;

		if (Profile.Num() < 3)
			return Projection;

		const FVector A = Points[Profile[0]];
		FVector B, C;
		bool bFound = false;

		for (int32 i = 1; i < Profile.Num() - 1; ++i)
		{
			B = Points[Profile[i]];
			C = Points[Profile[i + 1]];

			FVector Normal = FVector::CrossProduct(B - A, C - A);
			if (!Normal.IsNearlyZero())
			{
				Normal.Normalize();
				FVector AxisX = (B - A).GetSafeNormal();
				FVector AxisY = FVector::CrossProduct(Normal, AxisX).GetSafeNormal();

				Projection.Origin = A;
				Projection.AxisX = AxisX;
				Projection.AxisY = AxisY;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			// Fallback: default projection
			UE_LOG(LogTemp, Error, TEXT("Failed to find non-collinear points in profile! Projection may be invalid."));
			Projection.Origin = A;
			Projection.AxisX = FVector(1, 0, 0);
			Projection.AxisY = FVector(0, 1, 0);
		}

		return Projection;
	}

	bool IsClockwise(const TArray<FVector2D>& Points)
	{
		float Area = 0.0f;
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FVector2D& p0 = Points[i];
			const FVector2D& p1 = Points[(i + 1) % Points.Num()];
			Area += (p1.X - p0.X) * (p1.Y + p0.Y);
		}
		return Area > 0.0;
	}

	UPROPERTY()
	AActor* OwnerRef = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMaterial = nullptr;
	
	UPROPERTY()
	UMaterialInterface* BaseGlassMaterial = nullptr;

	const Model* ModelRef = nullptr;

};
