

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utils/FragmentsUtils.h"
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

	void SetModelGuid(const FString& InModelGuid) { ModelGuid = InModelGuid; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	FString GetModelGuid() const { return ModelGuid; }

	void SetLocalId(const int32 InLocalId) { LocalId = InLocalId; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	int32 GetLocalId() const { return LocalId; }

	void SetCategory(const FString& InCategory) { Category = InCategory; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	FString GetCategory() const { return Category; }

	void SetGuid(const FString& InGuid) { Guid = InGuid; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	FString GetGuid() const { return Guid; }

	void SetAttributes(TArray<struct FItemAttribute> InAttributes) { Attributes = InAttributes; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	TArray<struct FItemAttribute> GetAttributes();
	
	void SetChildren(TArray<AFragment*> InChildren) { FragmentChildren = InChildren; }
	
	void AddChild(AFragment* InChild) {
		if (InChild) FragmentChildren.Add(InChild);
	}

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	TArray<AFragment*> GetChildren() const { return FragmentChildren; }

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	AFragment* FindFragmentByLocalId(int32 InLocalId);

	UFUNCTION(BlueprintCallable, Category = "Fragments")
	const TArray<struct FFragmentSample>& GetSamples() const { return Samples; }

	void AddSampleInfo(const struct FFragmentSample& Sample) { Samples.Add(Sample); }
	void SetGlobalTransform(const FTransform& InGlobalTransform) { GlobalTransform = InGlobalTransform; }
	FTransform GetGlobalTransform() const { return GlobalTransform; }
	void SetData(FFragmentItem InFragmentItem);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fragments|Attributes")
	FString ModelGuid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fragments|Attributes")
	int32 LocalId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fragments|Attributes")
	FString Category;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fragments|Attributes")
	FString Guid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fragments|Attributes")
	TArray<struct FItemAttribute> Attributes;

private:

	FFragmentItem Data;

	
	UPROPERTY()
	TArray<AFragment*> FragmentChildren;

	UPROPERTY()
	TArray<struct FFragmentSample> Samples;

	UPROPERTY()
	FTransform GlobalTransform;
	
};
