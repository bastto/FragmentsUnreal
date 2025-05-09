


#include "Utils/FragmentsUtils.h"

FTransform UFragmentsUtils::MakeTransform(const Transform* FragmentsTransform, bool bIsLocalTransform)
{
	if (!FragmentsTransform)
	{
		UE_LOG(LogTemp, Error, TEXT("MakeTransform received null Transform pointer."));
		return FTransform::Identity;
	}

	// Sanitize input vectors
	FVector X = SafeVector(FVector(
		FragmentsTransform->x_direction().x(),
		FragmentsTransform->x_direction().z(),
		FragmentsTransform->x_direction().y()));

	FVector Z = SafeVector(FVector(
		FragmentsTransform->y_direction().x(),
		FragmentsTransform->y_direction().z(),
		FragmentsTransform->y_direction().y()));

	FVector Y = SafeVector(FVector::CrossProduct(Z, X)).GetSafeNormal(); // LH flip

	FVector Pos = SafeVector(FVector(
		FragmentsTransform->position().x(),
		FragmentsTransform->position().z(),
		FragmentsTransform->position().y())) * 100.0f;

	FMatrix RotMatrix(X, Y, Z, FVector::ZeroVector);
	FRotator Rotator = RotMatrix.Rotator();

	// Sanitize rotator too
	Rotator = SafeRotator(Rotator);

	FTransform OutTransform = FTransform(Rotator, Pos, FVector(1.0f));
	UE_LOG(LogTemp, Verbose, TEXT("Final FTransform: %s"), *OutTransform.ToString());
	return OutTransform;
}

FPlaneProjection UFragmentsUtils::BuildProjectionPlane(const TArray<FVector>& Points, const TArray<int32>& Profile)
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

bool UFragmentsUtils::IsClockwise(const TArray<FVector2D>& Points)
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

TArray<FItemAttribute> UFragmentsUtils::ParseItemAttribute(const Attribute* Attr)
{
	TArray<FItemAttribute> Parsed;
	if (!Attr || !Attr->data()) return Parsed;

	for (flatbuffers::uoffset_t i = 0; i < Attr->data()->size(); i++)
	{
		const auto* Raw = Attr->data()->Get(i);
		if (!Raw) continue;

		FString RawString = UTF8_TO_TCHAR(Raw->c_str());

		TArray<FString> Tokens;
		RawString.Replace(TEXT("["), TEXT(""))
			.Replace(TEXT("]"), TEXT(""))
			.ParseIntoArray(Tokens, TEXT(","), true);

		if (Tokens.Num() < 3) continue;

		FString Key = Tokens[0].TrimStartAndEnd().Replace(TEXT("\""), TEXT(""));
		FString Value = Tokens[1].TrimStartAndEnd().Replace(TEXT("\""), TEXT(""));
		int32 TypeHash = FCString::Atoi(*Tokens[2].TrimStartAndEnd());

		Parsed.Add(FItemAttribute(Key, Value, TypeHash));
	}
	return Parsed;
}

void UFragmentsUtils::PrintSpatialStructure(const SpatialStructure* spatial_struct, int32 ParentIndex)
{
	
	int32 currentIndex = ParentIndex + 1;
	UE_LOG(LogTemp, Log, TEXT("st: %d"), currentIndex);

	if (spatial_struct->local_id().has_value())
		UE_LOG(LogTemp, Log, TEXT("\tlocal_id: %d"), spatial_struct->local_id());

	if (spatial_struct->category())
	{
		if (spatial_struct->category()->size() > 0)
			UE_LOG(LogTemp, Log, TEXT("\tcategory: %s"), UTF8_TO_TCHAR(spatial_struct->category()->c_str()));
	}

	if (spatial_struct->children()->size() > 0)
	{
		for (flatbuffers::uoffset_t i = 0; i < spatial_struct->children()->size(); i++)
		{
			PrintSpatialStructure(spatial_struct->children()->Get(i), currentIndex);
		}
	}

	
}

float UFragmentsUtils::SafeComponent(float Value)
{
	return FMath::IsFinite(Value) ? Value : 0.0f;
}

FVector UFragmentsUtils::SafeVector(const FVector& Vec)
{
	return FVector(
		SafeComponent(Vec.X),
		SafeComponent(Vec.Y),
		SafeComponent(Vec.Z)
	);
}

FRotator UFragmentsUtils::SafeRotator(const FRotator& Rot)
{
	return FRotator(
		SafeComponent(Rot.Pitch),
		SafeComponent(Rot.Yaw),
		SafeComponent(Rot.Roll)
	);
}
