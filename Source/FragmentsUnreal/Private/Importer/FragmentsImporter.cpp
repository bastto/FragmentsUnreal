


#include "Importer/FragmentsImporter.h"
#include "flatbuffers/flatbuffers.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "ThirdParty/zlib/1.2.13/include/zlib.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "RawMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "StaticMeshDescription.h"
#include <StaticMeshOperations.h>
#include "Async/Async.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "CompGeom/PolygonTriangulation.h"
#include "tesselator.h"
#include "Algo/Reverse.h"

void UFragmentsImporter::Process(AActor* OwnerA, const FString& FragPath)
{
	SetOwnerRef(OwnerA);
	TArray<uint8> CompressedData;
	bool bIsCompressed = false;
	TArray<uint8> Decompressed;


	if (!FFileHelper::LoadFileToArray(CompressedData, *FragPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load the compressed file"));
		return;
	}

	if (CompressedData.Num() >= 2 && CompressedData[0] == 0x78)
	{
		bIsCompressed = true;
		UE_LOG(LogTemp, Log, TEXT("Zlib header detected. Starting decompression..."));
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
			UE_LOG(LogTemp, Error, TEXT("zlib initialization failed: %d"), ret);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("Starting decompression..."));

		const int32 ChunkSize = 1024 * 1024;
		int32 TotalOut = 0;

		for (int32 i = 0; i < 100; ++i)
		{
			int32 OldSize = Decompressed.Num();
			Decompressed.AddUninitialized(ChunkSize);
			stream.next_out = Decompressed.GetData() + OldSize;
			stream.avail_out = ChunkSize;

			ret = inflate(&stream, Z_NO_FLUSH);

			// Log out more detailed information for debugging
			UE_LOG(LogTemp, Log, TEXT("Decompressing chunk %d: avail_in = %d, avail_out = %d, ret = %d"), i, stream.avail_in, stream.avail_out, ret);

			if (ret == Z_STREAM_END)
			{
				TotalOut = OldSize + ChunkSize - stream.avail_out;
				break;
			}
			else if (ret != Z_OK)
			{
				UE_LOG(LogTemp, Error, TEXT("Decompression failed with error code: %d"), ret);
				break;
			}
		}

		ret = inflateEnd(&stream);
		if (ret != Z_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("zlib end stream failed: %d"), ret);
			return;
		}

		Decompressed.SetNum(TotalOut);
		UE_LOG(LogTemp, Log, TEXT("Decompression complete. Total bytes: %d"), TotalOut);
	}
	else
	{
		Decompressed = CompressedData;
		UE_LOG(LogTemp, Log, TEXT("Data appears uncompressed, using raw data"));
	}

	const Model* ModelRef = GetModel(Decompressed.GetData());
	//PrintModelStructure(ModelRef);

	if (!ModelRef)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse Fragments model"));
		return;
	}

	BaseGlassMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentGlassMaterial.M_BaseFragmentGlassMaterial"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/FragmentsUnreal/Materials/M_BaseFragmentMaterial.M_BaseFragmentMaterial"));


	// Loop through samples and spawn meshes
	const auto* _meshes = ModelRef->meshes();
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

		// samples: includes representations,
		for (flatbuffers::uoffset_t i = 0; i < samples->size(); i++)
		{
			const auto* sample = samples->Get(i);
			const auto mesh = meshes_items->Get(sample->item());
			const auto* material = materials->Get(sample->material());
			const auto* representation = representations->Get(sample->representation());
			const auto* local_transform = local_tranforms->Get(sample->local_transform());
			const auto* global_transform = global_transforms->Get(mesh); // testing getting the global transform based on the mesh item
			UStaticMesh* Mesh = nullptr;

			FString Name = FString::Printf(TEXT("frag_%d"), i);
			if (representation->representation_class() == RepresentationClass::RepresentationClass_SHELL)
			{
				const auto* shell = shells->Get(representation->id());
				Mesh = CreateStaticMeshFromShell(shell, material, *Name, TEXT("/Game/Fragments"));
				if (Mesh)
				{
					SpawnStaticMesh(Mesh,local_transform, global_transform, OwnerRef->GetWorld(), FName(TEXT("shell")));

				}
			}
			else if (representation->representation_class() == RepresentationClass_CIRCLE_EXTRUSION)
			{
				const auto* circleExtrusion = cirle_extrusions->Get(representation->id());
				Mesh = CreateStaticMeshFromCircleExtrusion(circleExtrusion, material, *Name, TEXT("/Game/Fragments"));
				if (Mesh)
				{
					SpawnStaticMesh(Mesh, local_transform, global_transform, OwnerRef->GetWorld(), FName(TEXT("circle_extrussion")));
				}
			}
			else
			{
				
			}
		}
	}
}

void UFragmentsImporter::SpawnStaticMesh(UStaticMesh* StaticMesh,const Transform* LocalTransform, const Transform* GlobalTransform, UWorld* World, FName OptionalTag)
{
	if (!StaticMesh || !LocalTransform || !GlobalTransform || !World) return;

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
	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);

	if (MeshActor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn StaticMeshActor."));
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

	// Ensure mesh is added and visible
	MeshComponent->SetVisibility(true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Add Optional tag
	MeshActor->Tags.Add(OptionalTag);
}

UStaticMesh* UFragmentsImporter::CreateStaticMeshFromShell(const Shell* ShellRef, const Material* RefMaterial, const FString& AssetName, const FString& AssetPath)
{
	// Create StaticMesh object
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(OwnerRef, FName(*AssetName), RF_Public | RF_Standalone | RF_Transient);
	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();

	UStaticMeshDescription* StaticMeshDescription = StaticMesh->CreateStaticMeshDescription(OwnerRef);
	FMeshDescription& MeshDescription = StaticMeshDescription->GetMeshDescription();
	UStaticMesh::FBuildMeshDescriptionsParams MeshParams;
	MeshParams.bBuildSimpleCollision = true;
	MeshParams.bCommitMeshDescription = true;
	MeshParams.bMarkPackageDirty = true;
	MeshParams.bUseHashAsGuid = false;

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

	FName MaterialSlotName = AddMaterialToMesh(StaticMesh, RefMaterial);
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
					UE_LOG(LogTemp, Log, TEXT("Invalid Indice: shell %s, profile %d, indice %d"), *AssetName, i, j);
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
				UE_LOG(LogTemp, Error, TEXT("Profile %d skipped: fewer than 3 points"), i);
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

			UE_LOG(LogTemp, Log, TEXT("Profile %d has %d points and %d holes"),
				i, Indices->size(), ProfileHolesIdx.Contains(i) ? ProfileHolesIdx[i].Num() : 0);

			TArray<int32> OutIndices;
			TArray<FVector> OutVertices;
			if (!TriangulatePolygonWithHoles(PointsRef, ProfilePointsIndex, ProfileHolesIdx[i], OutVertices, OutIndices))
			{
				UE_LOG(LogTemp, Error, TEXT("Profile %d skipped: Triangulation failed"), i);
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
	FAssetRegistryModule::AssetCreated(StaticMesh);

	return StaticMesh;
}

UStaticMesh* UFragmentsImporter::CreateStaticMeshFromCircleExtrusion(const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, const FString& AssetName, const FString& AssetPath)
{
	if (!CircleExtrusion || !CircleExtrusion->axes() || CircleExtrusion->axes()->size() == 0)
		return nullptr;

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(OwnerRef, FName(*AssetName), RF_Public | RF_Standalone | RF_Transient);
	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();

	TArray<const FMeshDescription*> MeshDescriptionPtrs;

	// LOD0 – Full circle extrusion
	UStaticMeshDescription* LOD0Desc = StaticMesh->CreateStaticMeshDescription(OwnerRef);
	BuildFullCircleExtrusion(*LOD0Desc, CircleExtrusion, RefMaterial, StaticMesh);
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

	StaticMesh->BuildFromMeshDescriptions(MeshDescriptionPtrs, MeshParams);
	
	if (StaticMesh->GetNumSourceModels() >= 3)
	{
		StaticMesh->GetSourceModel(0).ScreenSize.Default = 1.0f;
		StaticMesh->GetSourceModel(1).ScreenSize.Default = 0.5f;
		StaticMesh->GetSourceModel(2).ScreenSize.Default = 0.1f;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unexpected: Only %d LODs were created!"), StaticMesh->GetNumSourceModels());
	}
	FAssetRegistryModule::AssetCreated(StaticMesh);
	return StaticMesh;
}


FName UFragmentsImporter::AddMaterialToMesh(UStaticMesh*& CreatedMesh, const Material* RefMaterial)
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
		UE_LOG(LogTemp, Error, TEXT("Unable to load Base Material"));
		return FName();
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, CreatedMesh);
	if (!DynamicMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create dynamic material."));
		return FName();
	}

	if (HasTransparency)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), A);
	}

	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), FVector4(R, G, B, A));


	// Add Material
	return CreatedMesh->AddMaterial(DynamicMaterial);
}

bool UFragmentsImporter::TriangulatePolygonWithHoles(const TArray<FVector>& Points,
	const TArray<int32>& Profiles,
	const TArray<TArray<int32>>& Holes,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutIndices)
{
	TESStesselator* Tess = tessNewTess(nullptr);
	FPlaneProjection Projection = BuildProjectionPlane(Points, Profiles);
	
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
				UE_LOG(LogTemp, Error, TEXT("Contour has fewer than 3 points, skipping."));
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
				UE_LOG(LogTemp, Error, TEXT("Contour is colinear in 2D projection, skipping."));
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
			bool bClockwise = IsClockwise(Projected);
			UE_LOG(LogTemp, Log, TEXT("Contour winding is %s"), bClockwise ? TEXT("CW") : TEXT("CCW"));

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
		UE_LOG(LogTemp, Error, TEXT("tessTesselate failed."));
		tessDeleteTess(Tess);
		return false;
	}

	const int32 VertexCount = tessGetVertexCount(Tess);
	const TESSreal* Vertices = tessGetVertices(Tess);

	if (VertexCount == 0)
	{
		for (const int32& P : Profiles)
		{
			UE_LOG(LogTemp, Warning, TEXT("\tPoints of Vertex 0 X: %.6f, Y: %.6f, Z: %.6f"), Points[P].X, Points[P].Y, Points[P].Z);
		}

		int32 HoleIdx = 0;
		for (const auto& H : Holes)
		{
			for (const int32& P : H)
			{
				UE_LOG(LogTemp, Warning, TEXT("\tPoints of Hole %d 0 X: %.6f, Y: %.6f, Z: %.6f"), HoleIdx, Points[P].X, Points[P].Y, Points[P].Z);
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
	UE_LOG(LogTemp, Log, TEXT("Tessellated VertexCount: %d, ElementCount: %d"), VertexCount, ElementCount);

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

void UFragmentsImporter::BuildFullCircleExtrusion(UStaticMeshDescription& StaticMeshDescription, const CircleExtrusion* CircleExtrusion, const Material* RefMaterial, UStaticMesh* StaticMesh)
{
	FStaticMeshAttributes Attributes(StaticMeshDescription.GetMeshDescription());
	Attributes.Register();

	FName MaterialSlotName = AddMaterialToMesh(StaticMesh, RefMaterial);
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
