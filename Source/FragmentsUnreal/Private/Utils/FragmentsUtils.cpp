


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
