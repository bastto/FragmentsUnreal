

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Fragment.generated.h"

UCLASS()
class FRAGMENTSUNREAL_API AFragment : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFragment();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	void SetModelGuid(const FString& InModelGuid) { ModelGuid = InModelGuid; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	FString GetModelGuid() const { return ModelGuid; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	void SetLocalId(const int32 InLocalId) { LocalId = InLocalId; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	int32 GetLocalId() const { return LocalId; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	void SetCategory(const FString& InCategory) { Category = InCategory; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	FString GetCategory() const { return Category; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	void SetGuid(const FString& InGuid) { Guid = InGuid; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	FString GetGuid() const { return Guid; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	void SetAttributes(TArray<struct FItemAttribute> InAttributes) { Attributes = InAttributes; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	TArray<struct FItemAttribute> GetAttributes() { return Attributes; }

private:

	UPROPERTY()
	FString ModelGuid;

	UPROPERTY()
	int32 LocalId;

	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString Guid;

	UPROPERTY()
	TArray<struct FItemAttribute> Attributes;
	
	
};
