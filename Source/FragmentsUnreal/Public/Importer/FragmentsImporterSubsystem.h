

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Utils/FragmentsUtils.h"
#include "FragmentsImporterSubsystem.generated.h"

struct FComponentSavedState
{
    FName CollisionProfile;                 
    ECollisionEnabled::Type CollisionEnabled;
    bool bGenerateOverlapEvents = false;
    bool bTickEnabled = false;
    bool bHiddenInGame = false;
};

/**
 * 
 */
UCLASS()
class FRAGMENTSUNREAL_API UFragmentsImporterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable)
	FString LoadFragment(const FString& FragPath);
	
	UFUNCTION(BlueprintCallable)
	void UnloadFragment(const FString& ModelGuid);

	UFUNCTION(BlueprintCallable)
	FString ProcessFragment(AActor* OwnerActor, const FString& FragPath, TArray<class AFragment*>& OutFragments, bool bSaveMeshes, bool bUseDynamicMesh);

	UFUNCTION(BlueprintCallable)
	void ProcessLoadedFragment(const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh);

	UFUNCTION(BlueprintCallable)
	void ProcessLoadedFragmentItem(int32 InLocalId, const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh);

	UFUNCTION(BlueprintCallable)
	TArray<int32> GetElementsByCategory(const FString& InCategory, const FString& ModelGuid);

	UFUNCTION(BlueprintCallable)
	AFragment* GetItemByLocalId(int32 InLocalId, const FString& InModelGuid);

	UFUNCTION(BlueprintCallable)
	TArray<FItemAttribute> GetItemPropertySets(int32 LocalId, const FString& InModelGuid);
	
	FFragmentItem* GetFragmentItemByLocalId(int32 InLocalId, const FString& InModelGuid);
	void GetItemData(FFragmentItem* InFragmentItem);

	UFUNCTION(BlueprintCallable)
	AFragment* GetModelFragment(const FString& InModelGuid);

	UFUNCTION(BlueprintCallable)
	FTransform GetBaseCoordinates();

	FORCEINLINE const TMap<FString, class UFragmentModelWrapper*>& GetFragmentModels() const
	{
		return FragmentModels;
	}

    static void SetHierarchyVisible(AActor* Root, bool bVisible)
    {
        if (!IsValid(Root)) return;

        if (bVisible)
        {
            RestoreActor(Root);
        }
        else
        {
            SaveAndHideActor(Root);
        }

        // Recurse through attached actors (includes ChildActorComponents’ spawned actors)
        TArray<AActor*> Attached;
        Root->GetAttachedActors(Attached);        // direct children
        for (AActor* Child : Attached)
        {
            SetHierarchyVisible(Child, bVisible); // recursion
        }
    }

    // Convenience: query current visibility from the actor (not a single component).
    static bool IsHierarchyVisible(const AActor* Root)
    {
        return IsValid(Root) ? !Root->IsHidden() : false;
    }

protected:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	UPROPERTY()
	TMap<FString, class UFragmentModelWrapper*> FragmentModels;

	UPROPERTY()
	class UFragmentsImporter* Importer = nullptr;


    static TMap<TWeakObjectPtr<AActor>, bool>& ActorTickCache()
    {
        static TMap<TWeakObjectPtr<AActor>, bool> S;
        return S;
    }

    static TMap<TWeakObjectPtr<UPrimitiveComponent>, FComponentSavedState>& Cache()
    {
        static TMap<TWeakObjectPtr<UPrimitiveComponent>, FComponentSavedState> S;
        return S;
    }

    static void SaveAndHideActor(AActor* A)
    {
        if (!IsValid(A)) return;

        // Save original actor tick state once
        if (!ActorTickCache().Contains(A))
        {
            ActorTickCache().Add(A, A->IsActorTickEnabled());
        }

        A->SetActorTickEnabled(false);
        A->SetActorHiddenInGame(true);

        // Hide & disable collision for all primitive components
        TInlineComponentArray<UPrimitiveComponent*> PrimComps;
        A->GetComponents(PrimComps);
        for (UPrimitiveComponent* C : PrimComps)
        {
            if (!IsValid(C)) continue;

            // Cache only once
            if (!Cache().Contains(C))
            {
                FComponentSavedState State;
                State.CollisionProfile = C->GetCollisionProfileName();
                State.CollisionEnabled = C->GetCollisionEnabled();
                State.bGenerateOverlapEvents = C->GetGenerateOverlapEvents();
                State.bTickEnabled = C->IsComponentTickEnabled();
                State.bHiddenInGame = C->bHiddenInGame;
                Cache().Add(C, State);
            }

            // Turn everything off
            C->SetHiddenInGame(true);
            C->SetGenerateOverlapEvents(false);
            C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            C->SetComponentTickEnabled(false);
        }
    }

    static void RestoreActor(AActor* A)
    {
        if (!IsValid(A)) return;

        // Restore components first
        TInlineComponentArray<UPrimitiveComponent*> PrimComps;
        A->GetComponents(PrimComps);
        for (UPrimitiveComponent* C : PrimComps)
        {
            if (!IsValid(C)) continue;

            if (FComponentSavedState* Saved = Cache().Find(C))
            {
                C->SetHiddenInGame(Saved->bHiddenInGame);
                C->SetCollisionProfileName(Saved->CollisionProfile);
                C->SetCollisionEnabled(Saved->CollisionEnabled);
                C->SetGenerateOverlapEvents(Saved->bGenerateOverlapEvents);
                C->SetComponentTickEnabled(Saved->bTickEnabled);
                Cache().Remove(C);
            }
            else
            {
                // Best-effort if it was spawned after we hid: make it usable.
                C->SetHiddenInGame(false);
                C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                C->SetGenerateOverlapEvents(true);
                C->SetComponentTickEnabled(C->PrimaryComponentTick.bCanEverTick);
            }
        }

        // Restore actor-level flags
        const bool* bTick = ActorTickCache().Find(A);
        if (bTick) { A->SetActorTickEnabled(*bTick); ActorTickCache().Remove(A); }
        else { A->SetActorTickEnabled(A->PrimaryActorTick.bCanEverTick); }

        A->SetActorHiddenInGame(false);
    }

};
