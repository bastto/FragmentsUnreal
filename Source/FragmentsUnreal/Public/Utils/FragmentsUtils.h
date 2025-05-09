

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Index/index_generated.h"
#include "FragmentsUtils.generated.h"

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

USTRUCT(BlueprintType)
struct FItemAttribute
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Key;

	UPROPERTY(BlueprintReadOnly)
	FString Value;

	UPROPERTY(BlueprintReadOnly)
	int32 TypeHash;

	FItemAttribute() : Key(TEXT("")), Value(TEXT("")), TypeHash(0) {}
	FItemAttribute(const FString& InKey, const FString& InValue, int32 InTypeHash)
		:Key(InKey), Value(InValue), TypeHash(InTypeHash) {}
};


/**
 * 
 */
UCLASS()
class FRAGMENTSUNREAL_API UFragmentsUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	//UFUNCTION(BlueprintCallable, Category = "Fragments")
	static FTransform MakeTransform(const Transform* FragmentsTransform, bool bIsLocalTransform = false);
	static FPlaneProjection BuildProjectionPlane(const TArray<FVector>& Points, const TArray<int32>& Profile);
	static bool IsClockwise(const TArray<FVector2D>& Points);
	static TArray<FItemAttribute> ParseItemAttribute(const Attribute* Attr);
	static void PrintSpatialStructure(const SpatialStructure* spatial_struct, int32 ParentIndex = 0);

	static float SafeComponent(float Value);
	static FVector SafeVector(const FVector& Vec);
	static FRotator SafeRotator(const FRotator& Rot);
};
