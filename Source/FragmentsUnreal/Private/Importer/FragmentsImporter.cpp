


#include "Importer/FragmentsImporter.h"
#include "flatbuffers/flatbuffers.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "zlib.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "StaticMeshDescription.h"
#include <StaticMeshOperations.h>
#include "Async/Async.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "CompGeom/PolygonTriangulation.h"
#include "tesselator.h"
#include "Algo/Reverse.h"
#include "Fragment/Fragment.h"
#include "Importer/FragmentModelWrapper.h"
#include "UObject/SavePackage.h"
#include "Misc/ScopedSlowTask.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Materials/MaterialInterface.h"
#include "DynamicMesh/MeshNormals.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceConstant.h"



DEFINE_LOG_CATEGORY(LogFragments);

UFragmentsImporter::UFragmentsImporter()
{

}

FString UFragmentsImporter::Process(AActor* OwnerA, const FString& FragPath, TArray<AFragment*>& OutFragments, bool bSaveMeshes, bool bUseDynamicMesh)
{
	SetOwnerRef(OwnerA);
	FString ModelGuidStr = LoadFragment(FragPath);

	if (ModelGuidStr.IsEmpty())	return FString();
	
	UFragmentModelWrapper* Wrapper = *FragmentModels.Find(ModelGuidStr);
	const Model* ModelRef = Wrapper->GetParsedModel();

	BaseGlassMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentGlassMaterial.M_BaseFragmentGlassMaterial"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentMaterial.M_BaseFragmentMaterial"));

	FDateTime StartTime = FDateTime::Now();
	SpawnFragmentModel(Wrapper->GetModelItem(), OwnerRef, ModelRef->meshes(), bSaveMeshes, Wrapper, bUseDynamicMesh);
	UE_LOG(LogFragments, Warning, TEXT("Loaded model in [%s]s -> %s"), *(FDateTime::Now() - StartTime).ToString(), *ModelGuidStr);
	if (PackagesToSave.Num() > 0)
	{
		DeferredSaveManager.AddPackagesToSave(PackagesToSave);
		PackagesToSave.Empty();
	}
	
	return ModelGuidStr;
}

void UFragmentsImporter::GetItemData(AFragment*& InFragment)
{
	if (!InFragment || InFragment->GetModelGuid().IsEmpty()) return;

	if (FragmentModels.Contains(InFragment->GetModelGuid()))
	{
		UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InFragment->GetModelGuid());
		const Model* InModel = Wrapper->GetParsedModel();
		
		int32 ItemIndex = UFragmentsUtils::GetIndexForLocalId(InModel, InFragment->GetLocalId());
		if (ItemIndex == INDEX_NONE) return;
	
		// Attributes
		const auto* attribute = InModel->attributes()->Get(ItemIndex);
		TArray<FItemAttribute> ItemAttributes = UFragmentsUtils::ParseItemAttribute(attribute);
		InFragment->SetAttributes(ItemAttributes);

		// Category
		const auto* category = InModel->categories()->Get(ItemIndex);
		const char* RawCategory = category->c_str();
		FString CategorySty = UTF8_TO_TCHAR(RawCategory);
		InFragment->SetCategory(CategorySty);
		//ItemActor->Tags.Add(FName(CategorySty));

		// Guids
		const auto* item_guid = InModel->guids()->Get(ItemIndex);
		const char* RawGuid = item_guid->c_str();
		FString GuidStr = UTF8_TO_TCHAR(RawGuid);
		InFragment->SetGuid(GuidStr);
	}
}

void UFragmentsImporter::GetItemData(FFragmentItem* InFragmentItem)
{
	if (InFragmentItem->ModelGuid.IsEmpty()) return;

	if (FragmentModels.Contains(InFragmentItem->ModelGuid))
	{
		UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InFragmentItem->ModelGuid);
		const Model* InModel = Wrapper->GetParsedModel();

		int32 ItemIndex = UFragmentsUtils::GetIndexForLocalId(InModel, InFragmentItem->LocalId);
		flatbuffers::uoffset_t ii = ItemIndex;
		if (ItemIndex == INDEX_NONE) return;

		// Attributes
		if (ii < InModel->attributes()->size())
		{
			const auto* attribute = InModel->attributes()->Get(ItemIndex);
			TArray<FItemAttribute> ItemAttributes = UFragmentsUtils::ParseItemAttribute(attribute);
			InFragmentItem->Attributes = ItemAttributes;
		}

		// Category
		if (ii < InModel->categories()->size())
		{
			const auto* category = InModel->categories()->Get(ItemIndex);
			if (category)  // Null check for category
			{
				const char* RawCategory = category->c_str();
				FString CategorySty = UTF8_TO_TCHAR(RawCategory);
				InFragmentItem->Category = CategorySty;
			}
		}

		// Guids
		if (ii < InModel->guids()->size())
		{
			const auto* item_guid = InModel->guids()->Get(ItemIndex);
			if (item_guid)  
			{
				const char* RawGuid = item_guid->c_str();
				FString GuidStr = UTF8_TO_TCHAR(RawGuid);
				InFragmentItem->Guid = GuidStr;
			}
		}
	}
}

TArray<FItemAttribute> UFragmentsImporter::GetItemPropertySets(AFragment* InFragment)
{
	TArray<FItemAttribute> CollectedAttributes;
	if (!InFragment || InFragment->GetModelGuid().IsEmpty()) return CollectedAttributes;
	if (!FragmentModels.Contains(InFragment->GetModelGuid())) return CollectedAttributes;

	UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InFragment->GetModelGuid());
	const Model* InModel = Wrapper->GetParsedModel();
	if (!InModel) return CollectedAttributes;

	TSet<int32> Visited;
	CollectPropertiesRecursive(InModel, InFragment->GetLocalId(), Visited, CollectedAttributes);
	return UFragmentsUtils::ParsePropertySets(CollectedAttributes);
	//return CollectedAttributes;
}

TArray<FItemAttribute> UFragmentsImporter::GetItemPropertySets(FFragmentItem* InFragment)
{
	TArray<FItemAttribute> CollectedAttributes;
	if (InFragment->ModelGuid.IsEmpty()) return CollectedAttributes;
	if (!FragmentModels.Contains(InFragment->ModelGuid)) return CollectedAttributes;

	UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InFragment->ModelGuid);
	const Model* InModel = Wrapper->GetParsedModel();
	if (!InModel) return CollectedAttributes;

	TSet<int32> Visited;
	CollectPropertiesRecursive(InModel, InFragment->LocalId, Visited, CollectedAttributes);
	return UFragmentsUtils::ParsePropertySets(CollectedAttributes);
	//return CollectedAttributes;
}

TArray<FItemAttribute> UFragmentsImporter::GetItemPropertySets(int32 LocalId, const FString& InModelGuid)
{
	TArray<FItemAttribute> CollectedAttributes;
	if (InModelGuid.IsEmpty()) return CollectedAttributes;
	if (!FragmentModels.Contains(InModelGuid)) return CollectedAttributes;

	UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InModelGuid);
	const Model* InModel = Wrapper->GetParsedModel();
	if (!InModel) return CollectedAttributes;

	TSet<int32> Visited;
	CollectPropertiesRecursive(InModel, LocalId, Visited, CollectedAttributes);
	return UFragmentsUtils::ParsePropertySets(CollectedAttributes);
	//return CollectedAttributes;
}


AFragment* UFragmentsImporter::GetItemByLocalId(int32 LocalId, const FString& ModelGuid)
{
	if (ModelFragmentsMap.Contains(ModelGuid))
	{
		FFragmentLookup Lookup = *ModelFragmentsMap.Find(ModelGuid);

		if (Lookup.Fragments.Contains(LocalId))
		{
			return *Lookup.Fragments.Find(LocalId);
		}
	}
	return nullptr;
}

FFragmentItem* UFragmentsImporter::GetFragmentItemByLocalId(int32 LocalId, const FString& InModelGuid)
{
	if (FragmentModels.Contains(InModelGuid))
	{
		UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InModelGuid);
		FFragmentItem* FoundItem;
		if (Wrapper->GetModelItem().FindFragmentByLocalId(LocalId, FoundItem))
		{
			return FoundItem;
		}
	}
	return nullptr;
}

FString UFragmentsImporter::LoadFragment(const FString& FragPath)
{
	TArray<uint8> CompressedData;
	bool bIsCompressed = false;
	TArray<uint8> Decompressed;


	if (!FFileHelper::LoadFileToArray(CompressedData, *FragPath))
	{
		UE_LOG(LogFragments, Error, TEXT("Failed to load the compressed file"));
		return FString();
	}

	if (CompressedData.Num() >= 2 && CompressedData[0] == 0x78)
	{
		bIsCompressed = true;
		UE_LOG(LogFragments, Log, TEXT("Zlib header detected. Starting decompression..."));
	}

	if (bIsCompressed)
	{
		// Use zlib directly (Unreal's zlib.h)
		z_stream stream = {};
		stream.next_in = CompressedData.GetData();
		stream.avail_in = CompressedData.Num();

		int32 ret = inflateInit(&stream);
		if (ret != Z_OK)
		{
			UE_LOG(LogFragments, Error, TEXT("zlib initialization failed: %d"), ret);
			return FString();
		}

		const int32 ChunkSize = 1024 * 1024;
		int32 TotalOut = 0;

		for (int32 i = 0; i < 100; ++i)
		{
			int32 OldSize = Decompressed.Num();
			Decompressed.AddUninitialized(ChunkSize);
			stream.next_out = Decompressed.GetData() + OldSize;
			stream.avail_out = ChunkSize;

			ret = inflate(&stream, Z_NO_FLUSH);

			if (ret == Z_STREAM_END)
			{
				TotalOut = OldSize + ChunkSize - stream.avail_out;
				break;
			}
			else if (ret != Z_OK)
			{
				UE_LOG(LogFragments, Error, TEXT("Decompression failed with error code: %d"), ret);
				break;
			}
		}

		ret = inflateEnd(&stream);
		if (ret != Z_OK)
		{
			UE_LOG(LogFragments, Error, TEXT("zlib end stream failed: %d"), ret);
			return FString();
		}

		Decompressed.SetNum(TotalOut);
		//UE_LOG(LogFragments, Log, TEXT("Decompression complete. Total bytes: %d"), TotalOut);
	}
	else
	{
		Decompressed = CompressedData;
		UE_LOG(LogFragments, Log, TEXT("Data appears uncompressed, using raw data"));
	}

	UFragmentModelWrapper* Wrapper = NewObject<UFragmentModelWrapper>(this);
	Wrapper->LoadModel(Decompressed);
	const Model* ModelRef = Wrapper->GetParsedModel();

	if (!ModelRef)
	{
		UE_LOG(LogFragments, Error, TEXT("Failed to parse Fragments model"));
		return FString();
	}

	const auto* guid = ModelRef->guid();
	const char* RawModelGuid = guid->c_str();
	FString ModelGuidStr = UTF8_TO_TCHAR(RawModelGuid);
	const auto* local_ids = ModelRef->local_ids();
	const auto* _meshes = ModelRef->meshes();
	

	const auto* spatial_structure = ModelRef->spatial_structure();
	FTransform RootTransform =  UFragmentsUtils::MakeTransform(_meshes->coordinates());
	FVector RootOffset = FVector::ZeroVector;
	if (!bBaseCoordinatesInitialized)
	{
		BaseCoordinates = RootTransform;
		bBaseCoordinatesInitialized = true;
	}
	else
	{
		RootOffset = BaseCoordinates.GetLocation() - RootTransform.GetLocation();
	}

	//FTransform RootTransform =  FTransform::Identity;
	FFragmentItem FragmentItem;
	FragmentItem.Guid = ModelGuidStr;
	FragmentItem.ModelGuid = ModelGuidStr;
	FragmentItem.GlobalTransform = RootTransform;
	UFragmentsUtils::MapModelStructureToData(spatial_structure, FragmentItem, TEXT(""));

	Wrapper->SetModelItem(FragmentItem);
	FragmentModels.Add(ModelGuidStr, Wrapper);


	// Loop through samples and spawn meshes
	if (_meshes)
	{
		const auto* samples = _meshes->samples();
		const auto* representations = _meshes->representations();
		const auto* coordinates = _meshes->coordinates();
		const auto* meshes_items = _meshes->meshes_items();
		const auto* materials = _meshes->materials();
		const auto* cirle_extrusions = _meshes->circle_extrusions();
		const auto* shells = _meshes->shells();
		const auto* local_tranforms = _meshes->local_transforms();
		const auto* global_transforms = _meshes->global_transforms();

		// Grouping samples by Item ID
		TMap<int32, TArray<const Sample*>> SamplesByItem;
		for (flatbuffers::uoffset_t i = 0; i < samples->size(); i++)
		{
			const auto* sample = samples->Get(i);
			SamplesByItem.FindOrAdd(sample->item()).Add(sample);
		}

		for (const auto& Item : SamplesByItem)
		{
			int32 ItemId = Item.Key;

			const TArray<const Sample*> ItemSamples = Item.Value;

			const auto mesh = meshes_items->Get(ItemId);
			const auto local_id = local_ids->Get(ItemId);

			FFragmentItem* FoundFragmentItem = nullptr;
			if (!Wrapper->GetModelItem().FindFragmentByLocalId(local_id, FoundFragmentItem))
			{
				return FString();
			}

			GetItemData(FoundFragmentItem);

			const auto* global_transform = global_transforms->Get(mesh);
			FTransform GlobalTransform = UFragmentsUtils::MakeTransform(global_transform);
			GlobalTransform.AddToTranslation(2*RootOffset);
			FoundFragmentItem->GlobalTransform = GlobalTransform;

			for (int32 i = 0; i < ItemSamples.Num(); i++)
			{
				const Sample* sample = ItemSamples[i];
				const auto* material = materials->Get(sample->material());
				const auto* representation = representations->Get(sample->representation());
				const auto* local_transform = local_tranforms->Get(sample->local_transform());

				FFragmentSample SampleInfo;
				SampleInfo.SampleIndex = i;
				SampleInfo.LocalTransformIndex = sample->local_transform();
				SampleInfo.RepresentationIndex = sample->representation();
				SampleInfo.MaterialIndex = sample->material();

				FoundFragmentItem->Samples.Add(SampleInfo);
			}

		}
	}
	ModelFragmentsMap.Add(ModelGuidStr, FFragmentLookup());

	return ModelGuidStr;
}

void UFragmentsImporter::ProcessLoadedFragment(const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh)
{
	if (!InOwnerRef || !FragmentModels.Contains(InModelGuid)) return;

	SetOwnerRef(InOwnerRef);

	UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InModelGuid);
	const Model* ModelRef = Wrapper->GetParsedModel();

	BaseGlassMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentGlassMaterial.M_BaseFragmentGlassMaterial"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentMaterial.M_BaseFragmentMaterial"));
	
	FDateTime StartTime = FDateTime::Now();
	Wrapper->SetSpawnedFragment(SpawnFragmentModel(Wrapper->GetModelItem(), OwnerRef, ModelRef->meshes(), bInSaveMesh, Wrapper, bUseDynamicMesh));
	UE_LOG(LogFragments, Warning, TEXT("Loaded model in [%s]s -> %s"), *(FDateTime::Now() - StartTime).ToString(), *InModelGuid);
	if (PackagesToSave.Num() > 0)
	{
		DeferredSaveManager.AddPackagesToSave(PackagesToSave);
		PackagesToSave.Empty();
	}
}

void UFragmentsImporter::ProcessLoadedFragmentItem(int32 InLocalId, const FString& InModelGuid, AActor* InOwnerRef, bool bInSaveMesh, bool bUseDynamicMesh)
{
	FFragmentItem* Item = GetFragmentItemByLocalId(InLocalId, InModelGuid);

	if (!InOwnerRef) return;

	SetOwnerRef(InOwnerRef);

	UFragmentModelWrapper* Wrapper = *FragmentModels.Find(InModelGuid);
	const Model* ModelRef = Wrapper->GetParsedModel();

	BaseGlassMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentGlassMaterial.M_BaseFragmentGlassMaterial"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentMaterial.M_BaseFragmentMaterial"));

	FDateTime StartTime = FDateTime::Now();
	Wrapper->SetSpawnedFragment(SpawnFragmentModel(*Item, OwnerRef, ModelRef->meshes(), bInSaveMesh, Wrapper, bUseDynamicMesh));
	UE_LOG(LogFragments, Warning, TEXT("Loaded model in [%s]s -> %s"), *(FDateTime::Now() - StartTime).ToString(), *InModelGuid);
	if (PackagesToSave.Num() > 0)
	{
		DeferredSaveManager.AddPackagesToSave(PackagesToSave);
		PackagesToSave.Empty();
	}
}

TArray<int32> UFragmentsImporter::GetElementsByCategory(const FString& InCategory, const FString& ModelGuid)
{
	TArray<int32> LocalIds;

	if (FragmentModels.Contains(ModelGuid))
	{
		UFragmentModelWrapper* Wrapper = *FragmentModels.Find(ModelGuid);
		const Model* InModel = Wrapper->GetParsedModel();

		if (!InModel)
			return LocalIds;

		const auto* categories = InModel->categories();
		const auto* local_ids = InModel->local_ids();

		if (!categories || !local_ids)
			return LocalIds;

		for (flatbuffers::uoffset_t i = 0; i < categories->size(); i++)
		{
			const auto* category = categories->Get(i);
			FString CategoryName = UTF8_TO_TCHAR(category->c_str());

			if (CategoryName.Equals(InCategory, ESearchCase::IgnoreCase))
			{
				int32 LocalId = local_ids->Get(i);
				LocalIds.Add(LocalId);
			}
		}
	}
	return LocalIds;
}

void UFragmentsImporter::UnloadFragment(const FString& ModelGuid)
{
	if (FFragmentLookup* Lookup = ModelFragmentsMap.Find(ModelGuid))
	{
		for (TPair<int32, AFragment*> Obj : Lookup->Fragments)
		{
			Obj.Value->Destroy();
		}
		ModelFragmentsMap.Remove(ModelGuid);
	}

	if (UFragmentModelWrapper** WrapperPtr = FragmentModels.Find(ModelGuid))
	{
		UFragmentModelWrapper* Wrapper = *WrapperPtr;

		Wrapper = nullptr;
		FragmentModels.Remove(ModelGuid);
	}
}

AFragment* UFragmentsImporter::GetModelFragment(const FString& ModelGuid)
{
	if (FragmentModels.Contains(ModelGuid))
	{
		UFragmentModelWrapper* Wrapper = *FragmentModels.Find(ModelGuid);
		return Wrapper->GetSpawnedFragment();
	}
	return nullptr;
}

void UFragmentsImporter::CollectPropertiesRecursive(
	const Model* InModel,
	int32 StartLocalId,
	TSet<int32>& Visited,
	TArray<FItemAttribute>& OutAttributes)
{
	if (!InModel || Visited.Contains(StartLocalId)) return;
	Visited.Add(StartLocalId);

	const auto* relations = InModel->relations();
	const auto* attributes = InModel->attributes();
	const auto* relations_items = InModel->relations_items();

	for (flatbuffers::uoffset_t i = 0; i < relations_items->size(); i++)
	{
		if (relations_items->Get(i) != StartLocalId) continue;

		const auto* Relation = relations->Get(i);
		if (!Relation || !Relation->data()) continue;

		for (flatbuffers::uoffset_t j = 0; j < Relation->data()->size(); j++)
		{
			const char* RawStr = Relation->data()->Get(j)->c_str();
			FString Cleaned = UTF8_TO_TCHAR(RawStr);

			TArray<FString> Tokens;
			Cleaned.Replace(TEXT("["), TEXT(""))
				.Replace(TEXT("]"), TEXT(""))
				.ParseIntoArray(Tokens, TEXT(","), true);

			if (Tokens.Num() < 2) continue;

			FString RelationName = Tokens[0].TrimStartAndEnd().Replace(TEXT("\""), TEXT(""));

			// Only allow property-related relations
			if (!(RelationName.Equals(TEXT("IsDefinedBy")) || RelationName.Equals(TEXT("HasProperties")) || RelationName.Equals(TEXT("DefinesType"))))
				continue;

			for (int32 k = 1; k < Tokens.Num(); ++k)
			{
				int32 RelatedLocalId = FCString::Atoi(*Tokens[k].TrimStartAndEnd());
				if (Visited.Contains(RelatedLocalId)) continue;

				// Try resolving RelatedLocalId to attribute
				flatbuffers::uoffset_t AttrIndex = UFragmentsUtils::GetIndexForLocalId(InModel, RelatedLocalId);
				if (AttrIndex != INDEX_NONE && attributes && AttrIndex < attributes->size())
				{
					const auto* Attr = attributes->Get(AttrIndex);
					if (Attr)
					{
						TArray<FItemAttribute> Props = UFragmentsUtils::ParseItemAttribute(Attr);
						OutAttributes.Append(Props);
					}
				}

				// Recurse
				CollectPropertiesRecursive(InModel, RelatedLocalId, Visited, OutAttributes);
			}
		}
	}
}


void UFragmentsImporter::SpawnStaticMesh(UStaticMesh* StaticMesh,const Transform* LocalTransform, const Transform* GlobalTransform, AActor* Owner, FName OptionalTag)
{
	if (!StaticMesh || !LocalTransform || !GlobalTransform || !Owner) return;


	// Convert Fragments transform → Unreal FTransform
	FVector Local(LocalTransform->position().x() * 100, LocalTransform->position().z() * 100, LocalTransform->position().y() * 100); // Fix z-up and Unreal units
	FVector XLocal(LocalTransform->x_direction().x(), LocalTransform->x_direction().z(), LocalTransform->x_direction().y());
	FVector YLocal(LocalTransform->y_direction().x(), LocalTransform->y_direction().z(), LocalTransform->y_direction().y());
	FVector ZLocal = FVector::CrossProduct(XLocal, YLocal);

	FVector Global(GlobalTransform->position().x() * 100, GlobalTransform->position().z() * 100, GlobalTransform->position().y() * 100); // Fix z-up and Unreal units
	FVector XGlobal(GlobalTransform->x_direction().x(), GlobalTransform->x_direction().z(), GlobalTransform->x_direction().y());
	FVector YGlobal(GlobalTransform->y_direction().x(), GlobalTransform->y_direction().z(), GlobalTransform->y_direction().y());
	FVector ZGlobal = FVector::CrossProduct(XGlobal, YGlobal);

	FVector Pos = Global + Local;

	FMatrix GlobalMatrix(XGlobal, YGlobal, ZGlobal, FVector::ZeroVector);  // Global rotation matrix
	FMatrix LocalMatrix(XLocal, YLocal, ZLocal, FVector::ZeroVector);  // Local rotation matrix

	// Debug log the rotation matrices
	//UE_LOG(LogTemp, Log, TEXT("GlobalMatrix: X(%s) Y(%s) Z(%s)"), *XGlobal.ToString(), *YGlobal.ToString(), *ZGlobal.ToString());
	//UE_LOG(LogTemp, Log, TEXT("LocalMatrix: X(%s) Y(%s) Z(%s)"), *XLocal.ToString(), *YLocal.ToString(), *ZLocal.ToString());


	FMatrix FinalRotationMatrix = LocalMatrix.Inverse() * GlobalMatrix;
	//FMatrix FinalRotationMatrix = LocalMatrix.GetTransposed() * GlobalMatrix;

	//UE_LOG(LogTemp, Log, TEXT("FinalRotationMatrix: %s"), *FinalRotationMatrix.ToString());


	FRotator Rot = FinalRotationMatrix.Rotator();
	FVector FinalScale = FVector(1.0f, 1.0f, 1.0f);

	FTransform Transform(Rot, Pos, FinalScale);

	// Step 1: Spawn StaticMeshActor
	AStaticMeshActor* MeshActor = Owner->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);

	if (MeshActor == nullptr)
	{
		UE_LOG(LogFragments, Error, TEXT("Failed to spawn StaticMeshActor."));
		return;
	}

	// Step 2: Set Mesh Actor's mobility to movable (so it can be adjusted in runtime)
	MeshActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);

	// Step 3: Ensure the mesh component is available (create one if needed)
	UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
	if (!MeshComponent)
	{
		// If no mesh component exists, create and register one
		MeshComponent = NewObject<UStaticMeshComponent>(MeshActor);
		MeshComponent->SetupAttachment(MeshActor->GetRootComponent());
		MeshComponent->RegisterComponent();
	}

	// Step 4: Set the static mesh
	MeshComponent->SetStaticMesh(StaticMesh);

	// Step 5: Mark the actor as needing an update to refresh its mesh
	MeshActor->MarkComponentsRenderStateDirty();
	MeshActor->SetActorTransform(Transform); // Apply the final transform

	// Step 6: Save the StaticMesh used


	// Ensure mesh is added and visible
	MeshComponent->SetVisibility(true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Add Optional tag
	MeshActor->Tags.Add(OptionalTag);
}

void UFragmentsImporter::SpawnFragmentModel(AFragment* InFragmentModel, AActor* InParent, const Meshes* MeshesRef, bool bSaveMeshes)
{
	if (!InFragmentModel || !InParent || !MeshesRef) return;

	// 1. Root Component
	USceneComponent* RootSceneComponent = NewObject<USceneComponent>(InFragmentModel);
	RootSceneComponent->RegisterComponent();
	InFragmentModel->SetRootComponent(RootSceneComponent);
	RootSceneComponent->SetMobility(EComponentMobility::Movable);

	// 2. Set Transform and info
	InFragmentModel->SetActorTransform(InFragmentModel->GetGlobalTransform());
	InFragmentModel->AttachToActor(InParent, FAttachmentTransformRules::KeepWorldTransform);

#if WITH_EDITOR
	if (!InFragmentModel->GetCategory().IsEmpty())
		InFragmentModel->SetActorLabel(InFragmentModel->GetCategory());
#endif

	// 3. Create Meshes If Sample Exists
	const TArray<FFragmentSample>& Samples = InFragmentModel->GetSamples();
	if (Samples.Num() > 0)
	{
		for (int32 i = 0; i < Samples.Num(); i++)
		{
			const FFragmentSample& Sample = Samples[i];

			//FString PackagePath = FString::Printf(TEXT("/Game/Buildings/%s"), *InFragmentModel->GetModelGuid());
			FString MeshName = FString::Printf(TEXT("%d_%d"), InFragmentModel->GetLocalId(), i);
			FString PackagePath = TEXT("/Game/Buildings") / InFragmentModel->GetModelGuid()/ MeshName;
			const FString SamplePath = PackagePath + TEXT(".") + MeshName;

			FString UniquePackageName = FPackageName::ObjectPathToPackageName(PackagePath);
			FString PackageFileName = FPackageName::LongPackageNameToFilename(UniquePackageName, FPackageName::GetAssetPackageExtension());

			const Material* material = MeshesRef->materials()->Get(Sample.MaterialIndex);
			const Representation* representation = MeshesRef->representations()->Get(Sample.RepresentationIndex);
			const Transform* local_transform = MeshesRef->local_transforms()->Get(Sample.LocalTransformIndex);

			FTransform LocalTransform = UFragmentsUtils::MakeTransform(local_transform);

			UStaticMesh* Mesh = nullptr;
			//FString MeshName = FString::Printf(TEXT("sample_%d_%d"), InFragmentModel->GetLocalId(), i);
			if (MeshCache.Contains(SamplePath))
			{
				Mesh = MeshCache[SamplePath];
			}
			else if (FPaths::FileExists(PackageFileName))
			{
				UPackage* ExistingPackage = LoadPackage(nullptr, *PackagePath, LOAD_None);
				//UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshObjectPath));
				if (ExistingPackage)
				{
					Mesh = FindObject<UStaticMesh>(ExistingPackage, *MeshName);
				}
			}
			else
			{
				UPackage* MeshPackage = CreatePackage(*PackagePath);
				if (representation->representation_class() == RepresentationClass::RepresentationClass_SHELL)
				{
					const auto* shell = MeshesRef->shells()->Get(representation->id());
					Mesh = CreateStaticMeshFromShell(shell, material, *MeshName, MeshPackage, InFragmentModel->GetModelGuid());

				}
				else if (representation->representation_class() == RepresentationClass_CIRCLE_EXTRUSION)
				{
					const auto* circleExtrusion = MeshesRef->circle_extrusions()->Get(representation->id());
					Mesh = CreateStaticMeshFromCircleExtrusion(circleExtrusion, material, *MeshName, MeshPackage, InFragmentModel->GetModelGuid());
				}

				if (Mesh)
				{
					if (!FPaths::FileExists(PackageFileName) && bSaveMeshes)
					{
#if WITH_EDITOR
						MeshPackage->FullyLoad();

						Mesh->Rename(*MeshName, MeshPackage);
						Mesh->SetFlags(RF_Public | RF_Standalone);
						//Mesh->Build();
						MeshPackage->MarkPackageDirty();
						FAssetRegistryModule::AssetCreated(Mesh);

						FSavePackageArgs SaveArgs;
						SaveArgs.SaveFlags = RF_Public | RF_Standalone;

						PackagesToSave.Add(MeshPackage);
#endif
						//UPackage::SavePackage(MeshPackage, Mesh, *PackageFileName, SaveArgs);
					}
				}

				MeshCache.Add(SamplePath, Mesh);
			}

			if (Mesh)
			{

				// Add StaticMeshComponent to parent actor
				UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(InFragmentModel);
				MeshComp->SetStaticMesh(Mesh);
				MeshComp->SetRelativeTransform(LocalTransform); // local to parent
				MeshComp->AttachToComponent(RootSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
				MeshComp->RegisterComponent();
				InFragmentModel->AddInstanceComponent(MeshComp);
			}
		}
	}

	// 4. Recursively spawn child fragments
	for (AFragment* Child : InFragmentModel->GetChildren())
	{
		SpawnFragmentModel(Child, InFragmentModel, MeshesRef, bSaveMeshes);
	}
}

AFragment* UFragmentsImporter::SpawnFragmentModel(FFragmentItem InFragmentItem, AActor* InParent, const Meshes* MeshesRef, bool bSaveMeshes, UFragmentModelWrapper* InWrapperRef, bool bUseDynamicMesh)
{
	if (!InParent) return nullptr;
	bSaveMaterials = bSaveMeshes;
	// Create AFragment

	AFragment* FragmentModel = OwnerRef->GetWorld()->SpawnActor<AFragment>(
		AFragment::StaticClass(), InFragmentItem.GlobalTransform);

	// Root Component
	USceneComponent* RootSceneComponent = NewObject<USceneComponent>(FragmentModel);
	RootSceneComponent->RegisterComponent();
	FragmentModel->SetRootComponent(RootSceneComponent);
	RootSceneComponent->SetMobility(EComponentMobility::Movable);

	// Set Transform and Info
	FTransform RootTransform = UFragmentsUtils::MakeTransform(MeshesRef->coordinates());
	FragmentModel->SetActorTransform(InFragmentItem.GlobalTransform);
	FragmentModel->SetData(InFragmentItem);
	FragmentModel->AttachToActor(InParent, FAttachmentTransformRules::KeepRelativeTransform);

#if WITH_EDITOR
	if (!FragmentModel->GetCategory().IsEmpty())
		FragmentModel->SetActorLabel(FragmentModel->GetCategory());
#endif

	// Create Meshes If Sample Exists
	const TArray<FFragmentSample>& Samples = FragmentModel->GetSamples();
	if (Samples.Num() > 0)
	{
		for (int32 i = 0; i < Samples.Num(); i++)
		{
			const FFragmentSample& Sample = Samples[i];

			FString MeshName = FString::Printf(TEXT("%d_%d"), FragmentModel->GetLocalId(), i);
			FString PackagePath = TEXT("/Game/Buildings") / FragmentModel->GetModelGuid() / MeshName;
			const FString SamplePath = PackagePath + TEXT(".") + MeshName;

			FString UniquePackageName = FPackageName::ObjectPathToPackageName(PackagePath);
			FString PackageFileName = FPackageName::LongPackageNameToFilename(UniquePackageName, FPackageName::GetAssetPackageExtension());

			const Material* material = MeshesRef->materials()->Get(Sample.MaterialIndex);
			const Representation* representation = MeshesRef->representations()->Get(Sample.RepresentationIndex);
			const Transform* local_transform = MeshesRef->local_transforms()->Get(Sample.LocalTransformIndex);

			FTransform LocalTransform = UFragmentsUtils::MakeTransform(local_transform);

			if (bUseDynamicMesh)
			{
				FDynamicMesh3 DynamicMesh;	

				// Class Shell
				UPackage* MeshPackage = CreatePackage(*PackagePath);
				if (representation->representation_class() == RepresentationClass::RepresentationClass_SHELL)
				{
					const auto* shell = MeshesRef->shells()->Get(representation->id());
					DynamicMesh = CreateDynamicMeshFromShell(shell, material, *MeshName, MeshPackage);

				}
				else if (representation->representation_class() == RepresentationClass_CIRCLE_EXTRUSION)
				{
					const auto* circleExtrusion = MeshesRef->circle_extrusions()->Get(representation->id());
					DynamicMesh = CreateDynamicMeshFromCircleExtrusion(circleExtrusion, material, *MeshName, MeshPackage);
				}

				UDynamicMeshComponent* DynamicMeshComponent = NewObject<UDynamicMeshComponent>(FragmentModel);
				DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
				AddMaterialToDynamicMesh(DynamicMeshComponent, material, InWrapperRef, Sample.MaterialIndex);
				DynamicMeshComponent->SetRelativeTransform(LocalTransform); // local to parent
				DynamicMeshComponent->AttachToComponent(RootSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
				DynamicMeshComponent->RegisterComponent();
				DynamicMeshComponent->SetComplexAsSimpleCollisionEnabled(true);
				DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				DynamicMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);

				FragmentModel->AddInstanceComponent(DynamicMeshComponent);
			}
			else
			{

				UStaticMesh* Mesh = nullptr;
				if (MeshCache.Contains(SamplePath))
				{
					Mesh = MeshCache[SamplePath];
				}
				else if (FPaths::FileExists(PackageFileName))
				{
					UPackage* ExistingPackage = LoadPackage(nullptr, *PackagePath, LOAD_None);
					if (ExistingPackage)
					{
						Mesh = FindObject<UStaticMesh>(ExistingPackage, *MeshName);
					}
				}
				else
				{
					UPackage* MeshPackage = CreatePackage(*PackagePath);
					if (representation->representation_class() == RepresentationClass::RepresentationClass_SHELL)
					{
						const auto* shell = MeshesRef->shells()->Get(representation->id());
						Mesh = CreateStaticMeshFromShell(shell, material, *MeshName, MeshPackage, FragmentModel->GetModelGuid());

					}
					else if (representation->representation_class() == RepresentationClass_CIRCLE_EXTRUSION)
					{
						const auto* circleExtrusion = MeshesRef->circle_extrusions()->Get(representation->id());
						Mesh = CreateStaticMeshFromCircleExtrusion(circleExtrusion, material, *MeshName, MeshPackage, FragmentModel->GetModelGuid());
					}

					if (Mesh)
					{
						if (!FPaths::FileExists(PackageFileName) && bSaveMeshes)
						{
#if WITH_EDITOR
							MeshPackage->FullyLoad();
	
							Mesh->Rename(*MeshName, MeshPackage);
							Mesh->SetFlags(RF_Public | RF_Standalone);
							MeshPackage->MarkPackageDirty();
							FAssetRegistryModule::AssetCreated(Mesh);
	
							FSavePackageArgs SaveArgs;
							SaveArgs.SaveFlags = RF_Public | RF_Standalone;
	
							PackagesToSave.Add(MeshPackage);
#endif
							UPackage::SavePackage(MeshPackage, Mesh, *PackageFileName, SaveArgs);
						}
					}

					MeshCache.Add(SamplePath, Mesh);
				}

				if (Mesh)
				{

					// Add StaticMeshComponent to parent actor
					UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(FragmentModel);
					MeshComp->SetStaticMesh(Mesh);
					MeshComp->SetRelativeTransform(LocalTransform); // local to parent
					MeshComp->AttachToComponent(RootSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
					MeshComp->RegisterComponent();
					FragmentModel->AddInstanceComponent(MeshComp);
				}
			}
		} 
	}

	if (ModelFragmentsMap.Contains(InFragmentItem.ModelGuid))
	{
		ModelFragmentsMap[InFragmentItem.ModelGuid].Fragments.Add(InFragmentItem.LocalId, FragmentModel);
	}

	// Recursively spawn child fragments
	for (FFragmentItem* Child : InFragmentItem.FragmentChildren)
	{
		SpawnFragmentModel(*Child, FragmentModel, MeshesRef, bSaveMeshes, InWrapperRef, bUseDynamicMesh);
	}

	return FragmentModel;
}

UStaticMesh* UFragmentsImporter::CreateStaticMeshFromShell(const Shell* ShellRef, const Material* RefMaterial, const FString& AssetName, UObject* OuterRef, const FString& InModelGuid)
{
	// Create StaticMesh object
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(OuterRef, FName(*AssetName), RF_Public | RF_Standalone /*| RF_Transient*/);
	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();

	UStaticMeshDescription* StaticMeshDescription = StaticMesh->CreateStaticMeshDescription(OuterRef);
	FMeshDescription& MeshDescription = StaticMeshDescription->GetMeshDescription();
	UStaticMesh::FBuildMeshDescriptionsParams MeshParams;

	//Build Settings
#if WITH_EDITOR
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
		SrcModel.BuildSettings.bRecomputeNormals = true;
		SrcModel.BuildSettings.bRecomputeTangents = true;
		SrcModel.BuildSettings.bRemoveDegenerates = true;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.BuildSettings.bBuildReversedIndexBuffer = true;
		SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel.BuildSettings.bGenerateLightmapUVs = true;
		SrcModel.BuildSettings.SrcLightmapIndex = 0;
		SrcModel.BuildSettings.DstLightmapIndex = 1;
		SrcModel.BuildSettings.MinLightmapResolution = 64;
	}
#endif

	MeshParams.bBuildSimpleCollision = true;
	MeshParams.bCommitMeshDescription = true;
	MeshParams.bMarkPackageDirty = true;
	MeshParams.bUseHashAsGuid = false;
#if !WITH_EDITOR
	MeshParams.bFastBuild = true;
#endif

	// Convert Shell Geometry (vertices and triangles)
	const auto* Points = ShellRef->points();
	TArray<FVector> PointsRef;
	TArray<FVertexID> Vertices;
	Vertices.Reserve(Points->size());

	for (flatbuffers::uoffset_t i = 0; i < Points->size(); i++)
	{
		const auto& P = *Points->Get(i);
		const FVertexID VertId = StaticMeshDescription->CreateVertex();
		StaticMeshDescription->SetVertexPosition(VertId, FVector(P.x()*100, P.z()*100, P.y()*100)); // Fix Z-up, and Unreal Units from m to cm
		PointsRef.Add(FVector(P.x() * 100, P.z() * 100, P.y() * 100));
		Vertices.Add(VertId);

		//UE_LOG(LogTemp, Log, TEXT("\t\t\t\tpoint %d: x: %f, y:%f, z:%f"), i, P.x(), P.y(), P.z());
	}

	FName MaterialSlotName = AddMaterialToMesh(StaticMesh, RefMaterial, InModelGuid);
	const FPolygonGroupID PolygonGroupId = StaticMeshDescription->CreatePolygonGroup();
	StaticMeshDescription->SetPolygonGroupMaterialSlotName(PolygonGroupId, MaterialSlotName);

	// Map the holes and identify the profiles that has holes
	const auto* Holes = ShellRef->holes();
	TMap<int32, TArray<TArray<int32>>> ProfileHolesIdx;
	for (flatbuffers::uoffset_t j = 0; j < Holes->size(); j++)
	{
		const auto* Hole = Holes->Get(j);
		const auto* HoleIndices = Hole->indices();
		const auto Profile_id = Hole->profile_id();
		TArray<int32> HoleIdx;

		for (flatbuffers::uoffset_t k = 0; k < HoleIndices->size(); k++)
		{
			HoleIdx.Add(HoleIndices->Get(k));
		}

		if (ProfileHolesIdx.Contains(Profile_id))
		{
			ProfileHolesIdx[Profile_id].Add(HoleIdx);
		}
		else
		{
			TArray<TArray<int32>> HolesForProfile;
			HolesForProfile.Add(HoleIdx);
			ProfileHolesIdx.Add(Profile_id, HolesForProfile);
		}
	}
	// Create Faces (triangles)
	const auto* Profiles = ShellRef->profiles();
	TMap<int32, FPolygonID> PolygonMap;
	
	for (flatbuffers::uoffset_t i = 0; i < Profiles->size(); i++)
	{
		// Create Profile Polygons for those that has no holes reference
		//UE_LOG(LogTemp, Log, TEXT("Processing Profile %d"), i);
		if (!ProfileHolesIdx.Contains(i))
		{
			const ShellProfile* Profile = Profiles->Get(i);
			const auto* Indices = Profile->indices();

			TArray<FVertexInstanceID> VertexInstances;
			VertexInstances.Reserve(Indices->size());
			TArray<FVertexInstanceID> TriangleInstance;
			for (flatbuffers::uoffset_t j = 0; j < Indices->size(); j++)
			{
				const auto Indice = Indices->Get(j);
				if (Vertices.IsValidIndex(Indice))
				{
					TriangleInstance.Add(MeshDescription.CreateVertexInstance(Vertices[Indice]));
				}
				else
					UE_LOG(LogFragments, Log, TEXT("Invalid Indice: shell %s, profile %d, indice %d"), *AssetName, i, j);
			}


			if (!TriangleInstance.IsEmpty())
			{
				FPolygonID PolygonID = MeshDescription.CreatePolygon(PolygonGroupId, TriangleInstance, {});
			}
		}
		// Process profile with holes to create new polygon that fully represent the substraction of holes in profile
		else
		{
			const ShellProfile* Profile = Profiles->Get(i);
			const auto* Indices = Profile->indices();
			if (Indices->size() < 3)
			{
				UE_LOG(LogFragments, Error, TEXT("Profile %d skipped: fewer than 3 points"), i);
				continue;
			}

			TArray<FVector> ProfilePoints;
			TArray<int32> ProfilePointsIndex;
			for (flatbuffers::uoffset_t j = 0; j < Indices->size(); j++)
			{
				const auto Indice = Indices->Get(j);
				ProfilePointsIndex.Add(Indice);
				const auto* Point = Points->Get(Indice);
				FVector Vector = FVector(Point->x() * 100, Point->y() * 100, Point->z() * 100);
				ProfilePoints.Add(Vector);
			}

			TArray<TArray<FVector>> ProfileHolesPoints;

			/*UE_LOG(LogTemp, Log, TEXT("Profile %d has %d points and %d holes"),
				i, Indices->size(), ProfileHolesIdx.Contains(i) ? ProfileHolesIdx[i].Num() : 0);*/

			TArray<int32> OutIndices;
			TArray<FVector> OutVertices;
			if (!TriangulatePolygonWithHoles(PointsRef, ProfilePointsIndex, ProfileHolesIdx[i], OutVertices, OutIndices))
			{
				UE_LOG(LogFragments, Error, TEXT("Profile %d skipped: Triangulation failed"), i);
				continue;
			}

			TMap<int32, FVertexID> TempVertexMap;
			for (int32 j = 0; j < OutVertices.Num(); j++)
			{
				FVertexID VId = StaticMeshDescription->CreateVertex();
				StaticMeshDescription->SetVertexPosition(VId, OutVertices[j]);
				TempVertexMap.Add(j, VId);
			}

			for (int32 j = 0; j < OutIndices.Num(); j+=3)
			{
				TArray<FVertexInstanceID> Triangle;

				Triangle.Add(MeshDescription.CreateVertexInstance(TempVertexMap[OutIndices[j]]));
				Triangle.Add(MeshDescription.CreateVertexInstance(TempVertexMap[OutIndices[j+1]]));
				Triangle.Add(MeshDescription.CreateVertexInstance(TempVertexMap[OutIndices[j+2]]));
				
				if (!Triangle.IsEmpty())
					MeshDescription.CreatePolygon(PolygonGroupId, Triangle);
			}

		}
	}
	
	FStaticMeshOperations::ComputeTriangleTangentsAndNormals(MeshDescription);
	FStaticMeshOperations::ComputeTangentsAndNormals(MeshDescription, EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents);

	StaticMesh->BuildFromMeshDescriptions(TArray<const FMeshDescription*>{&MeshDescription}, MeshParams);

	return StaticMesh;
}

UStaticMesh* UFragmentsImporter::CreateStaticMeshFromCircleExtrusion(const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, const FString& AssetName, UObject* OuterRef, const FString& InModelGuid)
{
	if (!CircleExtrusion || !CircleExtrusion->axes() || CircleExtrusion->axes()->size() == 0)
		return nullptr;

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(OuterRef, FName(*AssetName), RF_Public | RF_Standalone | RF_Transient);
	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();

	TArray<const FMeshDescription*> MeshDescriptionPtrs;

	//Build Settings
#if WITH_EDITOR
	{
		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
		SrcModel.BuildSettings.bRecomputeNormals = true;
		SrcModel.BuildSettings.bRecomputeTangents = true;
		SrcModel.BuildSettings.bRemoveDegenerates = true;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.BuildSettings.bBuildReversedIndexBuffer = true;
		SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel.BuildSettings.bGenerateLightmapUVs = false;
		SrcModel.BuildSettings.SrcLightmapIndex = 0;
		SrcModel.BuildSettings.DstLightmapIndex = 1;
		SrcModel.BuildSettings.MinLightmapResolution = 64;
	}
#endif

	// LOD0 – Full circle extrusion
	UStaticMeshDescription* LOD0Desc = StaticMesh->CreateStaticMeshDescription(OuterRef);
	BuildFullCircleExtrusion(*LOD0Desc, CircleExtrusion, RefMaterial, StaticMesh, InModelGuid);
	MeshDescriptionPtrs.Add(&LOD0Desc->GetMeshDescription());

	{ // To Do: Implementation of LOD for Static Mesh Created. Seaking for better Performance??
		// LOD1 – Line representation
		//UStaticMeshDescription* LOD1Desc = StaticMesh->CreateStaticMeshDescription(OwnerRef);
		//BuildLineOnlyMesh(*LOD1Desc, CircleExtrusion);
		//MeshDescriptionPtrs.Add(&LOD1Desc->GetMeshDescription());

		//// LOD2 – Empty mesh
		//UStaticMeshDescription* LOD2Desc = StaticMesh->CreateStaticMeshDescription(OwnerRef);
		////BuildEmptyMesh(*LOD2Desc);
		//MeshDescriptionPtrs.Add(&LOD2Desc->GetMeshDescription());
	}

	// Build mesh
	UStaticMesh::FBuildMeshDescriptionsParams MeshParams;
	MeshParams.bBuildSimpleCollision = true;
	MeshParams.bCommitMeshDescription = true;
	MeshParams.bMarkPackageDirty = true;
	MeshParams.bUseHashAsGuid = false;

#if !WITH_EDITOR
	MeshParams.bFastBuild = true;
#endif

	StaticMesh->BuildFromMeshDescriptions(MeshDescriptionPtrs, MeshParams);

	// THIS IS EDITOR ONLY. TO DO: Find the way to make it in Runtime??
	//if (StaticMesh->GetNumSourceModels() >= 3)
	//{
	//	StaticMesh->GetSourceModel(0).ScreenSize.Default = 1.0f;
	//	StaticMesh->GetSourceModel(1).ScreenSize.Default = 0.5f;
	//	StaticMesh->GetSourceModel(2).ScreenSize.Default = 0.1f;
	//}
	//else
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("Unexpected: Only %d LODs were created!"), StaticMesh->GetNumSourceModels());
	//}
	return StaticMesh;
}

FDynamicMesh3 UFragmentsImporter::CreateDynamicMeshFromShell(const Shell* ShellRef, const Material* RefMaterial, const FString& AssetName, UObject* OuterRef)
{
	TArray<FVector> ShellPoints;
	{
		const auto* PointsFB = ShellRef->points();
		ShellPoints.Reserve(PointsFB->size());
		for (flatbuffers::uoffset_t i = 0; i < PointsFB->size(); ++i)
		{
			const auto& P = *PointsFB->Get(i);
			// note: Z‐up and meters→cm conversion
			ShellPoints.Add(FVector(P.x() * 100, P.z() * 100, P.y() * 100));
		}
	}

	// 2) Build the hole‐map exactly as in your static version
	TMap<int32, TArray<TArray<int32>>> ProfileHolesIdx;
	{
		const auto* HolesFB = ShellRef->holes();
		for (flatbuffers::uoffset_t h = 0; h < HolesFB->size(); ++h)
		{
			const auto* Hole = HolesFB->Get(h);
			const auto* Indices = Hole->indices();
			int32       ProfileId = Hole->profile_id();

			TArray<int32> HoleIdx;
			for (flatbuffers::uoffset_t k = 0; k < Indices->size(); ++k)
			{
				HoleIdx.Add(Indices->Get(k));
			}
			ProfileHolesIdx.FindOrAdd(ProfileId).Add(HoleIdx);
		}
	}

	// 3) Create an empty dynamic mesh and begin filling
	FDynamicMesh3 DynamicMesh;
	//DynamicMesh.staticme.EnableCompactCopyOnWrite();  // optional perf tweak
	const auto* ProfilesFB = ShellRef->profiles();
	for (flatbuffers::uoffset_t pi = 0; pi < ProfilesFB->size(); ++pi)
	{
		// 3a) collect the outer loop indices
		const auto* ProfileFB = ProfilesFB->Get(pi);
		const auto* IdxFB = ProfileFB->indices();
		int32       NumPts = IdxFB->size();
		if (NumPts < 3)
		{
			UE_LOG(LogFragments, Warning,
				TEXT("Profile %d has fewer than 3 points, skipping"), pi);
			continue;
		}

		TArray<int32> OuterLoop;
		OuterLoop.Reserve(NumPts);
		for (int32 j = 0; j < NumPts; ++j)
		{
			OuterLoop.Add(IdxFB->Get(j));
		}

		// 3b) fetch any hole loops (could be empty)
		TArray<TArray<int32>> Holes = ProfileHolesIdx.Contains(pi)
			? ProfileHolesIdx[pi]
			: TArray<TArray<int32>>();

			// 3c) triangulate
			TArray<FVector> OutVerts;     // 3D points
			TArray<int32>  OutIndices;    // flat [i0,i1,i2, i0,i1,i2, …]
			bool bOK = TriangulatePolygonWithHoles(
				ShellPoints,           // all shell points
				OuterLoop,             // our profile
				Holes,                 // holes
				OutVerts,              // out 3D verts
				OutIndices);           // out flat triangle list

			if (!bOK)
			{
				UE_LOG(LogFragments, Error,
					TEXT("Triangulation failed on profile %d"), pi);
				continue;
			}

			// 3d) append these new vertices and get back their new IDs
			TArray<int> NewVIDs;
			NewVIDs.SetNum(OutVerts.Num());
			for (int32 v = 0; v < OutVerts.Num(); ++v)
			{
				NewVIDs[v] = DynamicMesh.AppendVertex(OutVerts[v]);
			}

			// 3e) append triangles
			for (int32 t = 0; t < OutIndices.Num(); t += 3)
			{
				int a = NewVIDs[OutIndices[t + 0]];
				int b = NewVIDs[OutIndices[t + 1]];
				int c = NewVIDs[OutIndices[t + 2]];
				DynamicMesh.AppendTriangle(a, b, c);
			}
	}

	// 4) recompute normals & finalize
	DynamicMesh.CompactCopy(DynamicMesh);
	return DynamicMesh;
}

FDynamicMesh3 UFragmentsImporter::CreateDynamicMeshFromCircleExtrusion(const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, const FString& AssetName, UObject* OuterRef)
{
	if (!CircleExtrusion || !CircleExtrusion->axes() || CircleExtrusion->axes()->size() == 0)
	{
		return nullptr;
	}
	FDynamicMesh3 Mesh;
	
	const auto* Axes = CircleExtrusion->axes();
	const auto* Radii = CircleExtrusion->radius();
	const int32 SegmentCount = 16;

	for (flatbuffers::uoffset_t axisIndex = 0; axisIndex < Axes->size(); axisIndex++)
	{
		const auto* Axis = Axes->Get(axisIndex);
		const auto* Orders = Axis->order();
		const auto* Parts = Axis->parts();
		const auto* Wires = Axis->wires();
		const auto* WireSets = Axis->wire_sets();
		const auto* Curves = Axis->circle_curves();
		
		for (flatbuffers::uoffset_t i = 0; i < Orders->size(); i++)
		{
			int32 OrderIndex = Orders->Get(i);
			int32 PartIndex = Parts->Get(i);

			TArray<TArray<int>> AllRings;

			if (PartIndex == (int)AxisPartClass::AxisPartClass_CIRCLE_CURVE && Curves)
			{
				const float Radius = Radii->Get(axisIndex) * 100.0f;

				TArray<FVector> ArcCenters;
				TArray<FVector> ArcTangents;

				for (flatbuffers::uoffset_t c = 0; c < Curves->size(); ++c)
				{
					const auto* Circle = Curves->Get(c);
					FVector Center = FVector(Circle->position().x(), Circle->position().z(), Circle->position().y()) * 100.0f;
					FVector XDir = FVector(Circle->x_direction().x(), Circle->x_direction().z(), Circle->x_direction().y());
					FVector YDir = FVector(Circle->y_direction().x(), Circle->y_direction().z(), Circle->y_direction().y());
					float ApertureRad = FMath::DegreesToRadians(Circle->aperture());
					float ArcRadius = Circle->radius() * 100.0f;

					int32 ArcDivs = FMath::Clamp(FMath::RoundToInt(ApertureRad * ArcRadius * 0.05f), 4, 32);
					for (int32 j = 0; j <= ArcDivs; ++j)
					{
						float t = static_cast<float>(j) / ArcDivs;
						float angle = -ApertureRad / 2.0f + t * ApertureRad;
						FVector Pos = Center + ArcRadius * (FMath::Cos(angle) * XDir + FMath::Sin(angle) * YDir);
						ArcCenters.Add(Pos);
					}
				}

				// Compute tangents
				for (int32 j = 0; j < ArcCenters.Num(); ++j)
				{
					if (j == 0)
						ArcTangents.Add((ArcCenters[1] - ArcCenters[0]).GetSafeNormal());
					else if (j == ArcCenters.Num() - 1)
						ArcTangents.Add((ArcCenters.Last() - ArcCenters[j - 1]).GetSafeNormal());
					else
						ArcTangents.Add((ArcCenters[j + 1] - ArcCenters[j - 1]).GetSafeNormal());
				}

				// Initial frame from first tangent
				FVector PrevTangent = ArcTangents[0];
				FVector PrevX, PrevY;
				PrevTangent.FindBestAxisVectors(PrevX, PrevY);

				for (int32 k = 0; k < ArcCenters.Num(); ++k)
				{
					const FVector& Tangent = ArcTangents[k];
					FQuat AlignQuat = FQuat::FindBetweenNormals(PrevTangent, Tangent);
					FVector CurrX = AlignQuat.RotateVector(PrevX);
					FVector CurrY = AlignQuat.RotateVector(PrevY);

					TArray<int32> Ring;
					for (int32 j = 0; j < SegmentCount; ++j)
					{
						float Angle = 2.0f * PI * j / SegmentCount;
						FVector Offset = FMath::Cos(Angle) * CurrX + FMath::Sin(Angle) * CurrY;
						FVector Pos = ArcCenters[k] + Offset * Radius;

						if (Pos.ContainsNaN())
						{
							continue;  // skip bad vertex
						}

						int vid = Mesh.AppendVertex(Pos);
						Ring.Add(vid);
					}

					if (Ring.Num() == SegmentCount)  // only if we got a full ring
					{
						AllRings.Add(Ring);
					}
					PrevTangent = Tangent;
					PrevX = CurrX;
					PrevY = CurrY;
				}


				// Stitch rings
				for (int32 k = 0; k < AllRings.Num() - 1; ++k)
				{
					const auto& RingA = AllRings[k];
					const auto& RingB = AllRings[k + 1];

					for (int32 j = 0; j < SegmentCount; ++j)
					{
						int Next = (j + 1) % SegmentCount;

						FVertexID V00 = RingA[j];
						FVertexID V01 = RingB[j];
						FVertexID V10 = RingA[Next];
						FVertexID V11 = RingB[Next];

						Mesh.AppendTriangle(RingA[j], RingB[j], RingA[Next]);
						Mesh.AppendTriangle(RingA[Next], RingB[j], RingB[Next]);
					}
				}

				AllRings.Reset();
			}
			else if (PartIndex == (int)AxisPartClass::AxisPartClass_WIRE && Wires)
			{
				const auto* Wire = Wires->Get(OrderIndex);
				FVector P1 = FVector(Wire->p1().x(), Wire->p1().z(), Wire->p1().y()) * 100.0f;
				FVector P2 = FVector(Wire->p2().x(), Wire->p2().z(), Wire->p2().y()) * 100.0f;

				FVector Direction = (P2 - P1).GetSafeNormal();
				FVector XDir, YDir;
				Direction.FindBestAxisVectors(XDir, YDir);

				TArray<int32> Ring1, Ring2;

				for (int32 j = 0; j < SegmentCount; j++)
				{
					float Angle = 2.0f * PI * j / SegmentCount;
					FVector Offset = FMath::Cos(Angle) * XDir + FMath::Sin(Angle) * YDir;
					FVector V1 = P1 + Offset * Radii->Get(OrderIndex) * 100.0f;
					FVector V2 = P2 + Offset * Radii->Get(OrderIndex) * 100.0f;
				
					if (V1.ContainsNaN())
					{
						continue;  // skip bad vertex
					}

					if (V2.ContainsNaN())
					{
						continue;  // skip bad vertex
					}

					int ID1 = Mesh.AppendVertex(V1);
					int ID2 = Mesh.AppendVertex(V2);
					

					Ring1.Add(ID1);
					Ring2.Add(ID2);
				}

				if (Ring1.Num() == SegmentCount && Ring2.Num() == SegmentCount)
				{
					for (int32 j = 0; j < SegmentCount; ++j)
					{
						int32 Next = (j + 1) % SegmentCount;

						Mesh.AppendTriangle(Ring1[j], Ring2[j], Ring1[Next]);
						Mesh.AppendTriangle(Ring1[Next], Ring2[j], Ring2[Next]);
					}
				}
			}
			else if (PartIndex == (int)AxisPartClass::AxisPartClass_WIRE_SET && WireSets)
			{
				const auto* WSet = WireSets->Get(OrderIndex);
				const auto* Points = WSet->ps();

				if (!Points || Points->size() < 2)
					continue;

				TArray<TArray<int>> Rings;

				for (flatbuffers::uoffset_t p = 0; p < Points->size(); p++)
				{
					const auto& Pt = Points->Get(p);
					FVector Pos = FVector(Pt->x(), Pt->z(), Pt->y()) * 100.0f;

					// compute Tangent along polyline
					FVector Tangent;
					if (p == 0)
					{
						const auto& Next = Points->Get(p + 1);
						Tangent = (FVector(Next->x(), Next->z(), Next->y()) * 100.0f - Pos).GetSafeNormal();
					}
					else if (p == Points->size() - 1)
					{
						const auto& Prev = Points->Get(p - 1);
						Tangent = (Pos - FVector(Prev->x(), Prev->z(), Prev->y()) * 100.0f).GetSafeNormal();
					}
					else
					{
						const auto& Prev = Points->Get(p - 1);
						const auto& Next = Points->Get(p + 1);
						Tangent = (FVector(Next->x(), Next->z(), Next->y()) * 100.0f - FVector(Prev->x(), Prev->z(), Prev->y()) * 100.0f).GetSafeNormal();
					}

					// Frame
					FVector XDir, YDir;
					Tangent.FindBestAxisVectors(XDir, YDir);

					TArray<int> Ring;
					for (int32 j = 0; j < SegmentCount; j++)
					{
						float Angle = 2.0f * PI * j / SegmentCount;
						FVector Offset = FMath::Cos(Angle) * XDir + FMath::Sin(Angle) * YDir;
						FVector RingPos = Pos + Offset * Radii->Get(OrderIndex) * 100.0f;

						if (RingPos.ContainsNaN())
						{
							continue;  // skip bad vertex
						}

						int Vtx = Mesh.AppendVertex(RingPos);
						Ring.Add(Vtx);
					}

					// Connect rings
					for (int32 k = 0; k < Rings.Num() - 1; ++k)
					{
						const auto& RingA = Rings[k];
						const auto& RingB = Rings[k + 1];

						if (RingA.Num() == SegmentCount && RingB.Num() == SegmentCount)
						{
							for (int32 j = 0; j < SegmentCount; ++j)
							{
								int32 Next = (j + 1) % SegmentCount;

								FVertexID V00 = RingA[j];
								FVertexID V01 = RingB[j];
								FVertexID V10 = RingA[Next];
								FVertexID V11 = RingB[Next];


								Mesh.AppendTriangle(RingA[j], RingB[j], RingA[Next]);
								Mesh.AppendTriangle(RingA[Next], RingB[j], RingB[Next]);
							}
						}
					}
				}
			}
		}
	}
	Mesh.CompactCopy(Mesh);
	return Mesh;
}

FName UFragmentsImporter::AddMaterialToMesh(UStaticMesh*& CreatedMesh, const Material* RefMaterial, const FString& InModelGuid)
{
	if (!RefMaterial || !CreatedMesh)return FName();

	bool HasTransparency = false;
	float R = RefMaterial->r() / 255.f;
	float G = RefMaterial->g() / 255.f;
	float B = RefMaterial->b() / 255.f;
	float A = RefMaterial->a() / 255.f;

	UMaterialInterface* Material = nullptr;
	if (A < 1)
	{
		///Script/Engine.Material'/FragmentsUnreal/Materials/M_BaseFragmentMaterial.M_BaseFragmentMaterial'
		Material = BaseGlassMaterial;
		HasTransparency = true;
	}
	else
	{
		Material = BaseMaterial;
	}
	if (!Material)
	{
		UE_LOG(LogFragments, Error, TEXT("Unable to load Base Material"));
		return FName();
	}

#if WITH_EDITOR
	FString MaterialName = FString::Printf(TEXT("mat_%d_%d_%d_%d"),
		FMath::RoundToInt(R * 255),
		FMath::RoundToInt(G * 255),
		FMath::RoundToInt(B * 255),
		FMath::RoundToInt(A * 255));
	FString PackagePath = TEXT("/Game/Buildings") / InModelGuid / MaterialName;
	FString UniquePackageName = FPackageName::ObjectPathToPackageName(PackagePath);
	FString PackageFileName = FPackageName::LongPackageNameToFilename(UniquePackageName, FPackageName::GetAssetPackageExtension());
	const FString SamplePath = PackagePath + TEXT(".") + MaterialName;
	
	UMaterialInstanceConstant* MaterialInstance = nullptr;
	if (MaterialsCache.Contains(SamplePath))
	{
		MaterialInstance = MaterialsCache[SamplePath];
	}
	else if (FPaths::FileExists(PackageFileName))
	{
		UPackage* ExistingPackage = LoadPackage(nullptr, *PackagePath, LOAD_None);
		if (ExistingPackage)
		{
			MaterialInstance = FindObject<UMaterialInstanceConstant>(ExistingPackage, *MaterialName);
		}
	}
	else
	{
		UPackage* MaterialPackage = CreatePackage(*PackagePath);
		MaterialInstance = NewObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass());
		MaterialInstance->SetParentEditorOnly(Material);
		MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("BaseColor"), FLinearColor(R, G, B, A));
		if (HasTransparency)
		{
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Opacity"), A);
		}

		if (!FPaths::FileExists(PackageFileName) && bSaveMaterials)
		{
			MaterialPackage->FullyLoad();
			MaterialInstance->Rename(*MaterialName, MaterialPackage);
			MaterialInstance->SetFlags(RF_Public | RF_Standalone);
			MaterialPackage->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(MaterialInstance);
			FSavePackageArgs SaveArgs;
			SaveArgs.SaveFlags = RF_Public | RF_Standalone;
			PackagesToSave.Add(MaterialPackage);
			UPackage::SavePackage(MaterialPackage, MaterialInstance, *PackageFileName, SaveArgs);
		}

		MaterialsCache.Add(SamplePath, MaterialInstance);
	}

	return CreatedMesh->AddMaterial(MaterialInstance);
#else
	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, CreatedMesh);
	if (!DynamicMaterial)
	{
		UE_LOG(LogFragments, Error, TEXT("Failed to create dynamic material."));
		return FName();
	}

	if (HasTransparency)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), A);
	}

	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), FVector4(R, G, B, A));


	// Add Material
	return CreatedMesh->AddMaterial(DynamicMaterial);
#endif

}

void UFragmentsImporter::AddMaterialToDynamicMesh(UDynamicMeshComponent* InDynComp, const Material* RefMaterial, UFragmentModelWrapper* InWrapperRef, int32 InMaterialIndex)
{
	if (!RefMaterial || !InDynComp)
	{
		return;
	}

	UMaterialInstanceDynamic** FoundMid = InWrapperRef->GetMaterialsMap().Find(InMaterialIndex);
	UMaterialInstanceDynamic* Mid = nullptr;

	if (FoundMid)
	{
		Mid = *FoundMid;
	}
	else
	{

		// unpack RGBA from the flatbuffers Material
		float R = RefMaterial->r() / 255.f;
		float G = RefMaterial->g() / 255.f;
		float B = RefMaterial->b() / 255.f;
		float A = RefMaterial->a() / 255.f;

		// pick your base UMaterial for opaque vs. translucent
		UMaterialInterface* BaseMat = (A < 1.0f) ? BaseGlassMaterial : BaseMaterial;
		if (!BaseMat)
		{
			UE_LOG(LogFragments, Error, TEXT("Unable to load base material for dynamic mesh"));
			return;
		}

		// create a dynamic instance off the base
		Mid = UMaterialInstanceDynamic::Create(BaseMat, InDynComp);
		if (!Mid)
		{
			UE_LOG(LogFragments, Error, TEXT("Failed to create Dynamic Material Instance"));
			return;
		}

		// set the parameters you need
		if (A < 1.0f)
		{
			Mid->SetScalarParameterValue(TEXT("Opacity"), A);
		}
		Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(R, G, B, A));
		InWrapperRef->GetMaterialsMap().Add(InMaterialIndex, Mid);
	}


	// finally assign it to slot 0 of the dynamic‐mesh component
	InDynComp->SetMaterial(0, Mid);
}

bool UFragmentsImporter::TriangulatePolygonWithHoles(const TArray<FVector>& Points,
	const TArray<int32>& Profiles,
	const TArray<TArray<int32>>& Holes,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutIndices)
{
	TESStesselator* Tess = tessNewTess(nullptr);
	FPlaneProjection Projection = UFragmentsUtils::BuildProjectionPlane(Points, Profiles);
	
	auto AddContour = [&](const TArray<int32>& Indices, bool bIsHole)
		{
			TArray<FVector2D> Projected;
			TArray<float> Contour;

			for (int32 Index : Indices)
			{
				FVector2D P2d = Projection.Project(Points[Index]);
				Projected.Add(P2d);
			}

			if (Projected.Num() < 3)
			{
				UE_LOG(LogFragments, Error, TEXT("Contour has fewer than 3 points, skipping."));
				return;
			}

			// Check for colinearity
			bool bColinear = true;
			const FVector2D& A = Projected[0];

			for (int i = 1; i < Projected.Num() - 1; ++i)
			{
				FVector2D Dir1 = (Projected[i] - A).GetSafeNormal();
				FVector2D Dir2 = (Projected[i + 1] - Projected[i]).GetSafeNormal();
				if (!Dir1.Equals(Dir2, 0.001f))
				{
					bColinear = false;
					break;
				}
			}
			if (bColinear)
			{
				UE_LOG(LogFragments, Error, TEXT("Contour is colinear in 2D projection, skipping."));
				return;
			}

			TArray<FVector2D> UniqueProjected;
			UniqueProjected.Reserve(Projected.Num());

			for (int32 i = 0; i < Projected.Num(); ++i)
			{
				if (i == 0 || !Projected[i].Equals(Projected[i - 1], 0.001))
				{
					UniqueProjected.Add(Projected[i]);
				}
			}
			Projected = UniqueProjected;

			/*for (const FVector2D& P : Projected)
			{
				UE_LOG(LogTemp, Log, TEXT("\tProjected Point before winding check: X: %.3f, Y: %.3f"), P.X, P.Y);
			}*/

			// Fix winding
			bool bClockwise = UFragmentsUtils::IsClockwise(Projected);
			//UE_LOG(LogTemp, Log, TEXT("Contour winding is %s"), bClockwise ? TEXT("CW") : TEXT("CCW"));

			if (!bIsHole && bClockwise)
			{
				Algo::Reverse(Projected); // Outer should be CCW
			}
			else if (bIsHole && !bClockwise)
			{
				Algo::Reverse(Projected); // Holes should be CW
			}
			
			for (const FVector2D& P : Projected)
			{
				Contour.Add(P.X);
				Contour.Add(P.Y);
				//UE_LOG(LogTemp, Warning, TEXT("    X: %.6f, Y: %.6f"), P.X, P.Y);
			}

			tessAddContour(Tess, 2, Contour.GetData(), sizeof(float) * 2, Projected.Num());
		};

	AddContour(Profiles, false);

	for (const TArray<int32>& Hole : Holes)
	{
		AddContour(Hole, true);
	}

	if (!tessTesselate(Tess, TESS_WINDING_ODD, TESS_POLYGONS, 3, 2, nullptr))
	{
		UE_LOG(LogFragments, Error, TEXT("tessTesselate failed."));
		tessDeleteTess(Tess);
		return false;
	}

	const int32 VertexCount = tessGetVertexCount(Tess);
	const TESSreal* Vertices = tessGetVertices(Tess);

	if (VertexCount == 0)
	{
		for (const int32& P : Profiles)
		{
			UE_LOG(LogFragments, Warning, TEXT("\tPoints of Vertex 0 X: %.6f, Y: %.6f, Z: %.6f"), Points[P].X, Points[P].Y, Points[P].Z);
		}

		int32 HoleIdx = 0;
		for (const auto& H : Holes)
		{
			for (const int32& P : H)
			{
				UE_LOG(LogFragments, Warning, TEXT("\tPoints of Hole %d 0 X: %.6f, Y: %.6f, Z: %.6f"), HoleIdx, Points[P].X, Points[P].Y, Points[P].Z);
			}
			HoleIdx++;
		}
	}

	TMap<int32, int32> IndexRemap;

	for (int32 i = 0; i < VertexCount; i++)
	{
		FVector2D P2d(Vertices[i * 2], Vertices[i * 2 + 1]);
		FVector P3d = Projection.Unproject(P2d);
		IndexRemap.Add(i, OutVertices.Num());
		OutVertices.Add(P3d);
	}
	const int32* Indices = tessGetElements(Tess);
	const int32 ElementCount = tessGetElementCount(Tess);
	//UE_LOG(LogTemp, Log, TEXT("Tessellated VertexCount: %d, ElementCount: %d"), VertexCount, ElementCount);

	for (int32 i = 0; i < ElementCount; ++i)
	{
		const int32* Poly = &Indices[i * 3];
		for (int j = 0; j < 3; ++j)
		{
			int32 Idx = Poly[j];
			if (Idx != TESS_UNDEF)
			{
				OutIndices.Add(IndexRemap[Idx]);
			}
		}
	}

	tessDeleteTess(Tess);

	return true;
}

void UFragmentsImporter::BuildFullCircleExtrusion(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, UStaticMesh* StaticMesh, const FString& InModelGuid)
{
	FStaticMeshAttributes Attributes(StaticMeshDescription.GetMeshDescription());
	Attributes.Register();

	FName MaterialSlotName = AddMaterialToMesh(StaticMesh, RefMaterial, InModelGuid);
	const FPolygonGroupID PolygonGroupId = StaticMeshDescription.CreatePolygonGroup();
	StaticMeshDescription.SetPolygonGroupMaterialSlotName(PolygonGroupId, MaterialSlotName);

	FMeshDescription& MeshDescription = StaticMeshDescription.GetMeshDescription();

	const auto* Axes = CircleExtrusion->axes();
	const auto* Radii = CircleExtrusion->radius();
	int32 SegmentCount = 16;

	for (flatbuffers::uoffset_t axisIndex = 0; axisIndex < Axes->size(); ++axisIndex)
	{
		const auto* Axis = Axes->Get(axisIndex);
		const auto* Orders = Axis->order();
		const auto* Parts = Axis->parts();
		const auto* Wires = Axis->wires();
		const auto* WireSets = Axis->wire_sets();
		//TArray<TArray<FVertexID>> AllRings;

		for (flatbuffers::uoffset_t i = 0; i < Orders->size(); i++)
		{
			int32 OrderIndex = Orders->Get(i);
			int32 PartIndex = Parts->Get(i);

			TArray<TArray<FVertexID>> RingVertexIDs;

			// Handle CIRCLE_CURVE
			if (Axis->circle_curves() && PartIndex == (int)AxisPartClass::AxisPartClass_CIRCLE_CURVE)
			{
				const auto* Curves = Axis->circle_curves();
				const float Radius = Radii->Get(axisIndex) * 100.0f;

				TArray<FVector> ArcCenters;
				TArray<FVector> ArcTangents;

				// Sample arc
				for (flatbuffers::uoffset_t c = 0; c < Curves->size(); ++c)
				{
					const auto* Circle = Curves->Get(c);
					FVector Center = FVector(Circle->position().x(), Circle->position().z(), Circle->position().y()) * 100.0f;
					FVector XDir = FVector(Circle->x_direction().x(), Circle->x_direction().z(), Circle->x_direction().y());
					FVector YDir = FVector(Circle->y_direction().x(), Circle->y_direction().z(), Circle->y_direction().y());
					float ApertureRad = FMath::DegreesToRadians(Circle->aperture());
					float ArcRadius = Circle->radius() * 100.0f;

					int32 ArcDivs = FMath::Clamp(FMath::RoundToInt(ApertureRad * ArcRadius * 0.05f), 4, 32);
					for (int32 j = 0; j <= ArcDivs; ++j)
					{
						float t = static_cast<float>(j) / ArcDivs;
						float angle = -ApertureRad / 2.0f + t * ApertureRad;
						FVector Pos = Center + ArcRadius * (FMath::Cos(angle) * XDir + FMath::Sin(angle) * YDir);
						ArcCenters.Add(Pos);
					}
				}

				// Compute tangents
				for (int32 j = 0; j < ArcCenters.Num(); ++j)
				{
					if (j == 0)
						ArcTangents.Add((ArcCenters[1] - ArcCenters[0]).GetSafeNormal());
					else if (j == ArcCenters.Num() - 1)
						ArcTangents.Add((ArcCenters.Last() - ArcCenters[j - 1]).GetSafeNormal());
					else
						ArcTangents.Add((ArcCenters[j + 1] - ArcCenters[j - 1]).GetSafeNormal());
				}

				// Initial frame from first tangent
				FVector PrevTangent = ArcTangents[0];
				FVector PrevX, PrevY;
				PrevTangent.FindBestAxisVectors(PrevX, PrevY);

				TArray<TArray<FVertexID>> AllRings;

				for (int32 k = 0; k < ArcCenters.Num(); ++k)
				{
					const FVector& Tangent = ArcTangents[k];
					FQuat AlignQuat = FQuat::FindBetweenNormals(PrevTangent, Tangent);
					FVector CurrX = AlignQuat.RotateVector(PrevX);
					FVector CurrY = AlignQuat.RotateVector(PrevY);

					TArray<FVertexID> Ring;
					for (int32 j = 0; j < SegmentCount; ++j)
					{
						float Angle = 2.0f * PI * j / SegmentCount;
						FVector Offset = FMath::Cos(Angle) * CurrX + FMath::Sin(Angle) * CurrY;
						FVector Pos = ArcCenters[k] + Offset * Radius;

						FVertexID V = StaticMeshDescription.CreateVertex();
						StaticMeshDescription.SetVertexPosition(V, Pos);
						Ring.Add(V);
					}

					AllRings.Add(Ring);
					PrevTangent = Tangent;
					PrevX = CurrX;
					PrevY = CurrY;
				}

				// Stitch rings
				for (int32 k = 0; k < AllRings.Num() - 1; ++k)
				{
					const auto& RingA = AllRings[k];
					const auto& RingB = AllRings[k + 1];

					for (int32 j = 0; j < SegmentCount; ++j)
					{
						int32 Next = (j + 1) % SegmentCount;

						FVertexID V00 = RingA[j];
						FVertexID V01 = RingB[j];
						FVertexID V10 = RingA[Next];
						FVertexID V11 = RingB[Next];

						TArray<FVertexInstanceID> Tri1 = {
							MeshDescription.CreateVertexInstance(V00),
							MeshDescription.CreateVertexInstance(V01),
							MeshDescription.CreateVertexInstance(V10)
						};
						TArray<FVertexInstanceID> Tri2 = {
							MeshDescription.CreateVertexInstance(V10),
							MeshDescription.CreateVertexInstance(V01),
							MeshDescription.CreateVertexInstance(V11)
						};

						MeshDescription.CreatePolygon(PolygonGroupId, Tri1);
						MeshDescription.CreatePolygon(PolygonGroupId, Tri2);
					}
				}
			}

			else if (Wires && PartIndex == (int)AxisPartClass::AxisPartClass_WIRE)
			{
				// Handle Wire
				const auto* Wire = Wires->Get(OrderIndex);
				FVector P1 = FVector(Wire->p1().x(), Wire->p1().z(), Wire->p1().y()) * 100.0f;
				FVector P2 = FVector(Wire->p2().x(), Wire->p2().z(), Wire->p2().y()) * 100.0f;

				FVector Direction = (P2 - P1).GetSafeNormal();
				FVector XDir, YDir;
				Direction.FindBestAxisVectors(XDir, YDir);

				TArray<FVertexID> Ring1, Ring2;

				for (int32 j = 0; j < SegmentCount; j++)
				{
					float Angle = 2.0f * PI * j / SegmentCount;
					FVector Offset = FMath::Cos(Angle) * XDir + FMath::Sin(Angle) * YDir;
					FVector V1 = P1 + Offset * Radii->Get(OrderIndex) * 100.0f;
					FVector V2 = P2 + Offset * Radii->Get(OrderIndex) * 100.0f;

					FVertexID ID1 = StaticMeshDescription.CreateVertex();
					FVertexID ID2 = StaticMeshDescription.CreateVertex();

					StaticMeshDescription.SetVertexPosition(ID1, V1);
					StaticMeshDescription.SetVertexPosition(ID2, V2);

					Ring1.Add(ID1);
					Ring2.Add(ID2);
				}

				for (int32 j = 0; j < SegmentCount; ++j)
				{
					int32 Next = (j + 1) % SegmentCount;

					FVertexID V00 = Ring1[j];
					FVertexID V01 = Ring2[j];
					FVertexID V10 = Ring1[Next];
					FVertexID V11 = Ring2[Next];

					TArray<FVertexInstanceID> Tri1 = {
						StaticMeshDescription.CreateVertexInstance(V00),
						StaticMeshDescription.CreateVertexInstance(V01),
						StaticMeshDescription.CreateVertexInstance(V10)
					};

					TArray<FVertexInstanceID> Tri2 = {
						StaticMeshDescription.CreateVertexInstance(V10),
						StaticMeshDescription.CreateVertexInstance(V01),
						StaticMeshDescription.CreateVertexInstance(V11)
					};

					StaticMeshDescription.GetMeshDescription().CreatePolygon(PolygonGroupId, Tri1);
					StaticMeshDescription.GetMeshDescription().CreatePolygon(PolygonGroupId, Tri2);
				}

				return;
			}

			else if (WireSets && PartIndex == (int)AxisPartClass::AxisPartClass_WIRE_SET)
			{
				const auto* WSet = WireSets->Get(OrderIndex);
				const auto* Points = WSet->ps();

				if (!Points || Points->size() < 2)
					continue;

				TArray<TArray<FVertexID>> Rings;

				for (flatbuffers::uoffset_t p = 0; p < Points->size(); p++)
				{
					const auto& Pt = Points->Get(p);
					FVector Pos = FVector(Pt->x(), Pt->z(), Pt->y()) * 100.0f;

					// compute Tangent along polyline
					FVector Tangent;
					if (p == 0)
					{
						const auto& Next = Points->Get(p + 1);
						Tangent = (FVector(Next->x(), Next->z(), Next->y()) * 100.0f - Pos).GetSafeNormal();
					}
					else if (p == Points->size() - 1)
					{
						const auto& Prev = Points->Get(p - 1);
						Tangent = (Pos - FVector(Prev->x(), Prev->z(), Prev->y()) * 100.0f).GetSafeNormal();
					}
					else
					{
						const auto& Prev = Points->Get(p - 1);
						const auto& Next = Points->Get(p + 1);
						Tangent = (FVector(Next->x(), Next->z(), Next->y()) * 100.0f - FVector(Prev->x(), Prev->z(), Prev->y()) * 100.0f).GetSafeNormal();
					}

					// Frame
					FVector XDir, YDir;
					Tangent.FindBestAxisVectors(XDir, YDir);

					TArray<FVertexID> Ring;
					for (int32 j = 0; j < SegmentCount; j++)
					{
						float Angle = 2.0f * PI * j / SegmentCount;
						FVector Offset = FMath::Cos(Angle) * XDir + FMath::Sin(Angle) * YDir;
						FVector RingPos = Pos + Offset * Radii->Get(OrderIndex) * 100.0f;

						FVertexID Vtx = StaticMeshDescription.CreateVertex();
						StaticMeshDescription.SetVertexPosition(Vtx, RingPos);
						Ring.Add(Vtx);
					}

					// Connect rings
					for (int32 k = 0; k < Rings.Num() - 1; ++k)
					{
						const auto& RingA = Rings[k];
						const auto& RingB = Rings[k + 1];

						for (int32 j = 0; j < SegmentCount; ++j)
						{
							int32 Next = (j + 1) % SegmentCount;

							FVertexID V00 = RingA[j];
							FVertexID V01 = RingB[j];
							FVertexID V10 = RingA[Next];
							FVertexID V11 = RingB[Next];

							TArray<FVertexInstanceID> Tri1 = {
								MeshDescription.CreateVertexInstance(V00),
								MeshDescription.CreateVertexInstance(V01),
								MeshDescription.CreateVertexInstance(V10)
							};

							TArray<FVertexInstanceID> Tri2 = {
								MeshDescription.CreateVertexInstance(V10),
								MeshDescription.CreateVertexInstance(V01),
								MeshDescription.CreateVertexInstance(V11)
							};

							MeshDescription.CreatePolygon(PolygonGroupId, Tri1);
							MeshDescription.CreatePolygon(PolygonGroupId, Tri2);
						}
					}
				}
			}
		}
	}

	FStaticMeshOperations::ComputeTriangleTangentsAndNormals(StaticMeshDescription.GetMeshDescription());
	FStaticMeshOperations::ComputeTangentsAndNormals(StaticMeshDescription.GetMeshDescription(), EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents);
}

void UFragmentsImporter::BuildLineOnlyMesh(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion)
{
	FStaticMeshAttributes Attributes(StaticMeshDescription.GetMeshDescription());
	Attributes.Register();
	const FPolygonGroupID PolygonGroupId = StaticMeshDescription.CreatePolygonGroup();
	FMeshDescription& MeshDescription = StaticMeshDescription.GetMeshDescription();

	const auto* Axes = CircleExtrusion->axes();
	if (!Axes) return;

	for (auto Axis : *Axes)
	{
		const auto* Orders = Axis->order();
		const auto* Wires = Axis->wires();

		for (uint32 i = 0; i < Orders->size(); ++i)
		{
			const auto* Wire = Wires->Get(Orders->Get(i));
			FVector P1 = FVector(Wire->p1().x(), Wire->p1().z(), Wire->p1().y()) * 100.0f;
			FVector P2 = FVector(Wire->p2().x(), Wire->p2().z(), Wire->p2().y()) * 100.0f;

			FVertexID V0 = StaticMeshDescription.CreateVertex();
			FVertexID V1 = StaticMeshDescription.CreateVertex();
			StaticMeshDescription.SetVertexPosition(V0, P1);
			StaticMeshDescription.SetVertexPosition(V1, P2);

			TArray<FVertexInstanceID> Tri;
			FVertexInstanceID I0 = StaticMeshDescription.CreateVertexInstance(V0);
			FVertexInstanceID I1 = StaticMeshDescription.CreateVertexInstance(V1);
			FVertexInstanceID I2 = StaticMeshDescription.CreateVertexInstance(V1); // degenerate triangle
			Tri.Add(I0);
			Tri.Add(I1);
			Tri.Add(I2);

			StaticMeshDescription.GetMeshDescription().CreatePolygon(PolygonGroupId, Tri,{});
		}
	}
}

TArray<FVector> UFragmentsImporter::SampleRingPoints(const FVector& Center, const FVector XDir, const FVector& YDir, float Radius, int SegmentCount, float ApertureRadians)
{
	TArray<FVector> Ring;
	for (int32 i = 0; i <= SegmentCount; i++)
	{
		float t = (float)i / SegmentCount;
		float angle = -ApertureRadians / 2.0f + t * ApertureRadians;
		FVector Point = Center + Radius * (FMath::Cos(angle) * XDir + FMath::Sin(angle) * YDir);
		Ring.Add(Point);
	}
	return Ring;
}

void UFragmentsImporter::SavePackagesWithProgress(const TArray<UPackage*>& InPackagesToSave)
{
#if WITH_EDITOR
	if (PackagesToSave.Num() == 0)
		return;

	// Create a slow task with progress bar and allow cancel
	FScopedSlowTask SlowTask((float)PackagesToSave.Num(), FText::FromString(TEXT("Saving Static Meshes...")));
	SlowTask.MakeDialog(true);

	for (UPackage* Package : PackagesToSave)
	{
		if (SlowTask.ShouldCancel())
		{
			UE_LOG(LogFragments, Warning, TEXT("User canceled saving packages."));
			break;
		}

		FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.SaveFlags = RF_Public | RF_Standalone;

		bool bSuccess = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
		if (!bSuccess)
		{
			UE_LOG(LogFragments, Error, TEXT("Failed to save package: %s"), *Package->GetName());
		}
		else
		{
			UE_LOG(LogFragments, Log, TEXT("Saved package: %s"), *Package->GetName());
		}

		SlowTask.EnterProgressFrame(1.f);
	}
#else
	// Runtime: do not save packages, just log
	UE_LOG(LogFragments, Log, TEXT("Skipping package saving in runtime environment."));
#endif
}