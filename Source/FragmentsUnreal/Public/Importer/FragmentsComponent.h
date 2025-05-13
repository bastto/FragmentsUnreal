

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Utils/FragmentsUtils.h"
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
	FString TestImportFragmentFile(const FString& Path, TArray<AFragment*>& OutFragments);

	UFUNCTION(BlueprintCallable, Category = "Fragments|Importer")
	TArray<class AFragment*> GetFragmentActors();
		
	UFUNCTION(BlueprintCallable, Category = "Fragments|Importer")
	TArray<FItemAttribute> GetItemPropertySets(AFragment* InFragment);

	UFUNCTION(BlueprintCallable, Category = "Fragments|Importer")
	AFragment* GetItemByLocalId(int32 LocalId, const FString& ModelGuid);
	
};
