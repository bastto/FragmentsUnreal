


#include "Utils/FragmentsUtils.h"
#include "Fragment/Fragment.h"

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
		int64 TypeHash = FCString::Atoi(*Tokens[2].TrimStartAndEnd());

		Parsed.Add(FItemAttribute(Key, Value, TypeHash));
	}
	return Parsed;
}

AFragment* UFragmentsUtils::MapModelStructure(const SpatialStructure* InS, AFragment*& ParentActor, TMap<int32, AFragment*>& FragmentLookupMapRef, const FString& InheritedCategory)
{
	// Determine if this node should spawn an actor
	const bool bHasLocalId = InS->local_id().has_value();
	const bool bHasCategory = InS->category() && InS->category()->size() > 0;
	const FString ThisCategory = bHasCategory ? UTF8_TO_TCHAR(InS->category()->c_str()) : InheritedCategory;

	const bool bEffectiveCategory = bHasCategory || !InheritedCategory.IsEmpty();
	const bool bShouldSpawn = bHasLocalId && bEffectiveCategory;

	AFragment* FragmentActor = nullptr;

	if (bShouldSpawn)
	{
		FragmentActor = ParentActor->GetWorld()->SpawnActor<AFragment>(AFragment::StaticClass(), FTransform::Identity);

		if (bHasLocalId)
		{
			int32 LocalId = InS->local_id().value();
			FragmentActor->SetLocalId(LocalId);
			FragmentLookupMapRef.Add(LocalId, FragmentActor);
		}

		FragmentActor->SetCategory(ThisCategory);
		FragmentActor->SetModelGuid(ParentActor->GetModelGuid());

#if WITH_EDITOR
		{
			FragmentActor->SetActorLabel(ThisCategory);
		}
#endif

		ParentActor->AddChild(FragmentActor);
	}

	const FString NextInheritedCategory = (!FragmentActor && bHasCategory) ? ThisCategory : TEXT("");

	if (InS->children())
	{
		for (flatbuffers::uoffset_t i = 0; i < InS->children()->size(); i++)
		{
			MapModelStructure(InS->children()->Get(i), FragmentActor ? FragmentActor : ParentActor, FragmentLookupMapRef, NextInheritedCategory);
		}
	}

	return FragmentActor;
}

void UFragmentsUtils::MapModelStructureToData(const SpatialStructure* InS, FFragmentItem& ParentItem, const FString& InheritedCategory)
{
	// Determine if this node should be stored as a fragment data object
	const bool bHasLocalId = InS->local_id().has_value();
	const bool bHasCategory = InS->category() && InS->category()->size() > 0;
	const FString ThisCategory = bHasCategory ? UTF8_TO_TCHAR(InS->category()->c_str()) : InheritedCategory;

	const bool bEffectiveCategory = bHasCategory || !InheritedCategory.IsEmpty();
	const bool bShouldStore = bHasLocalId && bEffectiveCategory;

	FFragmentItem* FragmentItem = new FFragmentItem();
	// Only proceed if we need to store the fragment data
	if (bShouldStore)
	{
		// Create a new FFragmentItem

		// Populate the fragment item with data from the SpatialStructure
		FragmentItem->ModelGuid = ParentItem.ModelGuid;
		FragmentItem->LocalId = InS->local_id().value();
		FragmentItem->Category = ThisCategory;

		// Store this FragmentItem as a child of the parent fragment item
		ParentItem.FragmentChildren.Add(FragmentItem);

	}


	// Now process children recursively if any
	if (InS->children())
	{
		for (flatbuffers::uoffset_t i = 0; i < InS->children()->size(); i++)
		{
			// Recursively add child fragments
			MapModelStructureToData(InS->children()->Get(i), bShouldStore ? *ParentItem.FragmentChildren.Last() : ParentItem, ThisCategory);
		}
	}
}

FString UFragmentsUtils::GetIfcCategory(const int64 InTypeHash)
{
	static const TMap<int64, FString> IfcCategoryMap = {
		{ 3258342251, TEXT("IFCBUILDINGELEMENTPROXY") },
		{ 1549835152, TEXT("IFCANNOTATION") },
		{ 3693920615, TEXT("IFCROOF") },
		{ 1939436016, TEXT("IFCBUILDING") },
		{ 983778844,  TEXT("IFCSLAB") },
		{ 2586569798, TEXT("IFCFOOTING") },
		{ 3006469996, TEXT("IFCCOLUMN") },
		{ 4085436816, TEXT("IFCWALLSTANDARDCASE") },
		{ 3234207722, TEXT("IFCSTAIRFLIGHT") },
		{ 3792847397, TEXT("IFCFURNISHINGELEMENT") },
		{ 3722059766, TEXT("IFCOPENINGELEMENT") },
		{ 3812528620, TEXT("IFCSITE") },
		{ 3999184608, TEXT("IFCSTAIR") },
		{ 3047399987, TEXT("IFCDOOR") },
		{ 2587468083, TEXT("IFCFLOWTERMINAL") },
		{ 2016195849, TEXT("IFCWALL") },
		{ 2692852465, TEXT("IFCSPACE") },
		{ 2476758395, TEXT("IFCFLOWSEGMENT") },
		{ 2735952531, TEXT("IFCFLOWFITTING") },
		{ 2568828275, TEXT("IFCPLATE") },
		{ 1319467500, TEXT("IFCFLOWCONTROLLER") },
		{ 2998110625, TEXT("IFCBEAM") },
		{ 1858898314, TEXT("IFCMEMBER") },
		{ 1592764394, TEXT("IFCCOVERING") },
		{ 3420386420, TEXT("IFCTENDON") },
		{ 3351459610, TEXT("IFCREINFORCINGBAR") },
		{ 3648960775, TEXT("IFCWINDOW") },
		{ 3123201583, TEXT("IFCELEMENTASSEMBLY") },
		{ 3356537533, TEXT("IFCTRANSPORTELEMENT") },
		{ 3237983705, TEXT("IFCELECTRICALELEMENT") },
		{ 3250150772, TEXT("IFCELECTRICAPPLIANCE") },
		{ 3223608652, TEXT("IFCELECTRICDISTRIBUTIONBOARD") },
		{ 2791496458, TEXT("IFCELECTRICFLOWSTORAGEDEVICE") },
		{ 2196933786, TEXT("IFCELECTRICGENERATOR") },
		{ 3992818766, TEXT("IFCELECTRICMOTOR") },
		{ 2625861777, TEXT("IFCELECTRICTIMECONTROL") },
		{ 4014341074, TEXT("IFCFIRESUPPRESSIONTERMINAL") },
		{ 3351767623, TEXT("IFCSANITARYTERMINAL") },
		{ 3102286572, TEXT("IFCLIGHTFIXTURE") },
		{ 2150113622, TEXT("IFCSWITCHINGDEVICE") },
		{ 3020706450, TEXT("IFCDISTRIBUTIONFLOWELEMENT") },
		{ 2996378946, TEXT("IFCFURNITURE") },
		{ 2330826553, TEXT("IFCPILE") },
		{ 3277505449, TEXT("IFCCHIMNEY") },
		{ 1647198132, TEXT("IFCTRIM") },
		{ 3035534955, TEXT("IFCMOVINGWALKWAY") },
		{ 1988756489, TEXT("IFCSHADINGDEVICE") },
		{ 3063033404, TEXT("IFCRAILING") },
		{ 2882494310, TEXT("IFCFLOWTREATMENTDEVICE") },
		{ 3258399478, TEXT("IFCCOMMUNICATIONSAPPLIANCE") },
		{ 2705486713, TEXT("IFCDUCTSILENCER") },
		{ 2484214793, TEXT("IFCUNITARYCONTROLELEMENT") },
		{ 4028950474, TEXT("IFCMECHANICALFASTENER") },
		{ 2122156756, TEXT("IFCFASTENER") },
		{ 1513078977, TEXT("IFCDISCRETEACCESSORY") },
		{ 2904403302, TEXT("IFCREINFORCINGMESH") },
		{ 1864285793, TEXT("IFCTENDONANCHOR") },
		{ 3465791920, TEXT("IFCVIBRATIONISOLATOR") },
		{ 2402280193, TEXT("IFCSTRUCTURALCURVEACTION") },
		{ 1481989373, TEXT("IFCSTRUCTURALCURVECONNECTION") },
		{ 2556467911, TEXT("IFCSTRUCTURALCURVEMEMBER") },
		{ 2028047011, TEXT("IFCSTRUCTURALLOADGROUP") },
		{ 2284384606, TEXT("IFCSTRUCTURALPLANARACTION") },
		{ 4010788049, TEXT("IFCSTRUCTURALPOINTCONNECTION") },
		{ 2974137194, TEXT("IFCSTRUCTURALPOINTLOAD") },
		{ 2094780014, TEXT("IFCSTRUCTURALPOINTREACTION") },
		{ 2934528095, TEXT("IFCSTRUCTURALRESULTGROUP") },
		{ 1340962872, TEXT("IFCSTRUCTURALSURFACEACTION") },
		{ 2002803610, TEXT("IFCSTRUCTURALSURFACECONNECTION") },
		{ 2750787162, TEXT("IFCSTRUCTURALSURFACEMEMBER") }
	};
	const FString* Found = IfcCategoryMap.Find(InTypeHash);

	return Found ? *Found : TEXT("UNKNOWN");
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

int32 UFragmentsUtils::GetIndexForLocalId(const Model* InModelRef, int32 LocalId)
{
	if (!InModelRef || !InModelRef->local_ids()) return INDEX_NONE;

	const auto* LocalIds = InModelRef->local_ids();
	for (flatbuffers::uoffset_t i = 0; i < LocalIds->size(); i++)
	{
		if (LocalIds->Get(i) == LocalId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}
