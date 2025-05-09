

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FragmentsComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FRAGMENTSUNREAL_API UFragmentsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UFragmentsComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY()
	class UFragmentsImporter* FragmentsImporter = nullptr;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Fragments|Importer")
	void TestImportFragmentFile(const FString& Path);

	UFUNCTION(BlueprintCallable, Category = "Fragments|Importer")
	TArray<class AFragment*> GetFragmentActors();

		
	
};
