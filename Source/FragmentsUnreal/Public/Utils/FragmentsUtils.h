

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Index/index_generated.h"
#include "FragmentsUtils.generated.h"

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

	static float SafeComponent(float Value);
	static FVector SafeVector(const FVector& Vec);
	static FRotator SafeRotator(const FRotator& Rot);
};
