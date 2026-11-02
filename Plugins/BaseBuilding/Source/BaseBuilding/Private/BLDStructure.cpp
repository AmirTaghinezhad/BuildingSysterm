// Fill out your copyright notice in the Description page of Project Settings.


#include "BLDStructure.h"

#include "BLDComponet.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "EditorGizmos/GizmoRotationUtil.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Transform/TransformConstraintUtil.h"

// Sets default values
ABLDStructure::ABLDStructure()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	BuildingMesh->SetupAttachment(RootComponent);

	ValidationBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ValidationBox"));
	ValidationBox->SetupAttachment(RootComponent);
	ValidationBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ValidationBox->SetGenerateOverlapEvents(false);
	ValidationBox->SetHiddenInGame(true);
}

// Called when the game starts or when spawned
void ABLDStructure::BeginPlay()
{
	Super::BeginPlay();
	if (!BLDComponent) {UE_LOG(LogTemp, Warning, TEXT("BLDStructure: BLDComponet Is not Valid"));}
	
	FBoxSphereBounds Bounds = BuildingMesh->GetStaticMesh()->GetBounds();

	//PlacementBounds->SetBoxExtent(Bounds.BoxExtent + FVector(-40, -30, -10));
	
}

void ABLDStructure::InitGhost()
{
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetWorldTimerManager().SetTimer(CycleHandle, this, &ABLDStructure::UpdateBuildPreview, 0.01f, true);
}

void ABLDStructure::StopGhost()
{
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetWorld()->GetTimerManager().ClearTimer(CycleHandle);
}

void ABLDStructure::UpdateBuildPreview()
{
		BuildCycle();
		ModifyBuildTransform(BuildTransform);
		SetActorTransform(BuildTransform);
	
}

void ABLDStructure::BuildCycle()
{
	//Trace to find a location to check for
	BuildTransform.SetLocation(BLDComponent->GhostTrace());
	
	float StructureClosestDistance = 1000;
	float ScoketClosestDistance = 1000;
	FTransform MySocketTransform;
	FTransform OtherSocketTransform;
	bool bSocketFaceEachOther = false;
	FCollisionShape SphereShape =
	FCollisionShape::MakeSphere(SnapRadius);
	ABLDStructure* SnappedStructure = nullptr;
	FName UsedSocketName;
	FName TargetSocketName;
		
	TArray<FOverlapResult> StructureResults;

	GetWorld()->OverlapMultiByObjectType(
		StructureResults,
		BuildTransform.GetLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		SphereShape
	);
	ABLDStructure* TargetStructure = nullptr;
	for (const FOverlapResult& StructureResult : StructureResults)
	{
		ABLDStructure* Other = Cast<ABLDStructure>(StructureResult.GetActor());

		if (!Other) continue;
		
		if (Other == this) continue;
		for (const FSnapSocket& Socket : SnapSockets)
		{
			if (!Other->BuildingMesh->DoesSocketExist(Socket.CompatibleSocket))
				continue;

			FVector ExtrnlSocketLocation = Other->BuildingMesh->GetSocketLocation(Socket.CompatibleSocket);
		
			float Distance = FVector::Dist(BuildTransform.GetLocation(), ExtrnlSocketLocation);
			if (Distance < ScoketClosestDistance)
			{
				ScoketClosestDistance = Distance;
				SnappedStructure = Other;
				
				UsedSocketName = Socket.SocketName;
				TargetSocketName = Socket.CompatibleSocket;
				MySocketTransform = BuildingMesh->GetSocketTransform(Socket.SocketName);
				OtherSocketTransform = Other->BuildingMesh->GetSocketTransform(Socket.CompatibleSocket);
				bSocketFaceEachOther = Socket.bFaceEachOther;
			}
		}
	}
	
	//constexpr float SnapDistance = 200.f; //Change this whenever SnapRaduis is changes

	if (ScoketClosestDistance < SnapRadius)
	{
		FTransform TargetSocketTransform = OtherSocketTransform;
		if (bSocketFaceEachOther)
		{
			FQuat Flip180(TargetSocketTransform.GetUnitAxis(EAxis::Z), PI);

			TargetSocketTransform.SetRotation(Flip180 * TargetSocketTransform.GetRotation());
		}
		// Transform of my socket relative to my actor.
		FTransform MySocketRelative = MySocketTransform.GetRelativeTransform(GetActorTransform());

		// Final actor transform that aligns MySocket to the other socket/ RotationOffset is checked so player manual rotation is not reversed 
		BuildTransform = MySocketRelative.Inverse() * TargetSocketTransform;
		if(FMath::Abs(RotationOffset.Yaw) >= 180.f) BuildTransform.SetRotation(BuildTransform.GetRotation() * RotationOffset.Quaternion().GetNormalized());
	}
	else
	{
		BuildTransform.SetRotation(BuildTransform.GetRotation() * RotationOffset.Quaternion().GetNormalized());
		RotationOffset = FRotator::ZeroRotator;
		SnappedStructure = nullptr;
	}
	
	//bool bBlocked = ValidateBuild(IsValid(SnappedStructure));
	bool bNewCanBuild = ValidateBuild(
	IsValid(SnappedStructure),
	SnappedStructure,
	UsedSocketName,
	TargetSocketName
);
	if (!bBuildStateInitialized || bNewCanBuild != bCanBuild)
	{
		//bBuildStateInitialized is used To avoid a first frame confliction caused by bCanBuild default value being the same as the first frame status, thus refusing if-clause 
		bBuildStateInitialized = true;
		bCanBuild = bNewCanBuild;
		//BLDComponent->OnBuildUpdate.Broadcast(bCanBuild);
		UpdatePreview_BlueprintFunction(bCanBuild);
	}
	
	
}

void ABLDStructure::Trace()
{
	FVector StartLocation = BLDComponent->CameraRef->GetComponentLocation() +  BLDComponent->CameraRef->GetForwardVector() * 350.0f;
	FVector EndLocation = BLDComponent->CameraRef->GetComponentLocation() +  BLDComponent->CameraRef->GetForwardVector() * 750.0f;
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(GetOwner());

	bool Hit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, CollisionParams);

	if (Hit)
	{
		BuildTransform.SetLocation(HitResult.ImpactPoint);
		UE_LOG(LogTemp, Log, TEXT("BLDStructure: Hit is true, Hit locations is %s"), *HitResult.ImpactNormal.ToString());
		
	}
	else
	{
		BuildTransform.SetLocation(HitResult.TraceEnd);
		UE_LOG(LogTemp, Log, TEXT("BLDStructure: Hit is false"));
	}
	DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red,false, 5.0f);
}

void ABLDStructure::RotationOfset(float ZOffset)
{
	/*if (!bCanFlip) return;
	FRotator Rot = GetActorRotation();
	Rot.Yaw += 180.f;
	SetActorRotation(Rot);*/
	RotationOffset.Yaw += ZOffset;
}

void ABLDStructure::ModifyBuildTransform_Implementation(FTransform& InOutTransform)
{
}

bool ABLDStructure::ValidateBuild(
	bool HasSnapped,
	ABLDStructure* SnappedStructure,
	FName UsedSocketName,
	FName TargetSocketName
)
{
	//if this structure should be snapped but is not
	if (!HasSnapped && bShouldSnap) return false;
	
	FCollisionShape Shape =
	FCollisionShape::MakeBox(ValidationBox->GetScaledBoxExtent());

	FCollisionQueryParams Params;
	//Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(this);
	//Params.AddIgnoredActor(SnappedStructure);
	
	const FVector BoxLocation = BuildTransform.TransformPosition( ValidationBox->GetRelativeLocation());
	const FQuat BoxRotation = BuildTransform.GetRotation() * ValidationBox->GetRelativeRotation().Quaternion();
	
	TArray<FOverlapResult> Results;

	bool bBlocked =
		GetWorld()->OverlapMultiByChannel(
			Results,
			BoxLocation,
			BoxRotation,
			ECC_WorldStatic,
			Shape,
			Params
		);

	DrawDebugBox(GetWorld(), BoxLocation, ValidationBox->GetScaledBoxExtent(),BoxRotation, FColor::Red, false, 5.0f);
	if (bBlocked) return false;
	
	if (!ValidateBuild_BlueprintSide( SnappedStructure, UsedSocketName, TargetSocketName))
		return false;
	
	return true;
	
}

bool ABLDStructure::ValidateBuild_BlueprintSide_Implementation(    
	ABLDStructure* SnappedStructure,
	FName UsedSocketName,
	FName TargetSocketName)
{
	return true;
}

float ABLDStructure::GetHealth()
{
	return CurrentHealth;
}

FName ABLDStructure::GetStructureName()
{
	return StructureName;
}

void ABLDStructure::Add_DepleatHealth(float Health)
{
}

void ABLDStructure::UpgradeStructure()
{
	const FBuildingInfo* UpgradeStructureData = BLDComponent->GetUpgradelStructureData(StructureName);
	
	if (!UpgradeStructureData) return;
	BuildingMesh->EmptyOverrideMaterials();
	BuildingMesh->SetStaticMesh(UpgradeStructureData->StructureMesh);
	CurrentHealth = UpgradeStructureData->MaxHealth;
	StructureName = UpgradeStructureData->StructureName;
}

void ABLDStructure::UpdatePreview_BlueprintFunction_Implementation(bool CanBuild)
{
}

void ABLDStructure::OnStructureBuilt_Implementation()
{
}

