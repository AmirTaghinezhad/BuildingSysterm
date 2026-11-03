#include "BLDStructure.h"

#include "BLDComponet.h"
#include "BuildingBlock.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABLDStructure::ABLDStructure()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	BuildingMesh->SetupAttachment(RootComponent);
	ValidationBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ValidationBox"));
	ValidationBox->SetupAttachment(RootComponent);
	ValidationBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ValidationBox->SetGenerateOverlapEvents(false);
	ValidationBox->SetHiddenInGame(true);
}

void ABLDStructure::BeginPlay()
{
	Super::BeginPlay();
	if (bIsbuilt && HasAuthority() && CurrentHealth <= 0.f) ApplyStructureData();
}

void ABLDStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABLDStructure, bIsbuilt);
	DOREPLIFETIME(ABLDStructure, bCanBuild);
	DOREPLIFETIME(ABLDStructure, StructureName);
	DOREPLIFETIME(ABLDStructure, BuildingOwner);
	DOREPLIFETIME(ABLDStructure, OwningBuildingBlock);
	DOREPLIFETIME(ABLDStructure, CurrentHealth);
}

void ABLDStructure::InitializeBuiltStructure(UDataTable* InDataTable, APlayerState* InOwner, ABuildingBlock* InBlock)
{
	if (!HasAuthority()) return;
	BuildingDataTable = InDataTable;
	BuildingOwner = InOwner;
	OwningBuildingBlock = InBlock;
	bIsbuilt = true;
	bCanBuild = true;
	ApplyStructureData();
	if (OwningBuildingBlock) OwningBuildingBlock->RecalculateUpkeepRequirements();
	ForceNetUpdate();
}

void ABLDStructure::ApplyStructureData()
{
	if (!BuildingDataTable) return;
	const FBuildingInfo* Data = BuildingDataTable->FindRow<FBuildingInfo>(StructureName, TEXT("ApplyStructureData"));
	if (!Data) return;
	if (Data->StructureMesh) BuildingMesh->SetStaticMesh(Data->StructureMesh);
	CurrentHealth = Data->MaxHealth;
}

bool ABLDStructure::GetUpkeepRequirement(FName& OutResource, float& OutAmount) const
{
	OutResource = NAME_None;
	OutAmount = 0.f;
	if (!BuildingDataTable) return false;
	const FBuildingInfo* Data = BuildingDataTable->FindRow<FBuildingInfo>(StructureName, TEXT("GetUpkeepRequirement"));
	if (!Data) return false;

	OutResource = Data->UpkeepResource.IsNone() ? Data->RequiredResource : Data->UpkeepResource;
	const float Percentage = FMath::Max(0.f, Data->UpkeepPercentage);
	OutAmount = FMath::Max(0.f, Data->RequiredAmount) * Percentage / 100.f;
	return !OutResource.IsNone() && OutAmount > 0.f;
}

void ABLDStructure::ApplyUpkeepDecay(float PercentageOfMaxHealth)
{
	if (!HasAuthority() || !bIsbuilt || PercentageOfMaxHealth <= 0.f) return;
	const FBuildingInfo* Data = BuildingDataTable
		? BuildingDataTable->FindRow<FBuildingInfo>(StructureName, TEXT("ApplyUpkeepDecay"))
		: nullptr;
	const float MaxHealth = Data ? Data->MaxHealth : CurrentHealth;
	if (MaxHealth <= 0.f) return;

	CurrentHealth = FMath::Max(0.f, CurrentHealth - MaxHealth * PercentageOfMaxHealth / 100.f);
	ForceNetUpdate();
	if (CurrentHealth <= 0.f)
	{
		if (OwningBuildingBlock) OwningBuildingBlock->DestroyStructure(this);
		else Destroy();
	}
}

void ABLDStructure::UpgradeStructure(UDataTable* DataTable)
{
	if (!HasAuthority() || !bIsbuilt) return;
	UDataTable* SourceTable = DataTable ? DataTable : BuildingDataTable;
	if (!SourceTable) return;
	const FBuildingInfo* CurrentData = SourceTable->FindRow<FBuildingInfo>(StructureName, TEXT("UpgradeStructure"));
	if (!CurrentData || CurrentData->NextLevelStructureRowName.IsNone()) return;
	const FBuildingInfo* NextData = SourceTable->FindRow<FBuildingInfo>(CurrentData->NextLevelStructureRowName, TEXT("UpgradeStructure"));
	if (!NextData) return;

	BuildingDataTable = SourceTable;
	StructureName = NextData->StructureName;
	if (NextData->StructureMesh) BuildingMesh->SetStaticMesh(NextData->StructureMesh);
	CurrentHealth = NextData->MaxHealth;
	if (OwningBuildingBlock) OwningBuildingBlock->RecalculateUpkeepRequirements();
	ForceNetUpdate();
}

void ABLDStructure::OnRep_CurrentHealth() {}
void ABLDStructure::OnRep_StructureName() { ApplyStructureData(); }

void ABLDStructure::InitGhost()
{
	SetReplicates(false);
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetWorldTimerManager().SetTimer(CycleHandle, this, &ABLDStructure::UpdateBuildPreview, 0.01f, true);
}

void ABLDStructure::StopGhost()
{
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetWorldTimerManager().ClearTimer(CycleHandle);
}

void ABLDStructure::UpdateBuildPreview()
{
	BuildCycle();
	ModifyBuildTransform(BuildTransform);
	SetActorTransform(BuildTransform);
}

void ABLDStructure::BuildCycle()
{
	if (!BLDComponent) return;
	BuildTransform.SetLocation(BLDComponent->GhostTrace());

	float SocketClosestDistance = 1000.f;
	FTransform MySocketTransform, OtherSocketTransform;
	bool bSocketFaceEachOther = false;
	ABLDStructure* SnappedStructure = nullptr;
	FName UsedSocketName, TargetSocketName;
	TArray<FOverlapResult> StructureResults;
	GetWorld()->OverlapMultiByObjectType(StructureResults, BuildTransform.GetLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic), FCollisionShape::MakeSphere(SnapRadius));

	for (const FOverlapResult& Result : StructureResults)
	{
		ABLDStructure* Other = Cast<ABLDStructure>(Result.GetActor());
		if (!Other || Other == this || !Other->bIsbuilt) continue;
		for (const FSnapSocket& Socket : SnapSockets)
		{
			if (!Other->BuildingMesh || !Other->BuildingMesh->DoesSocketExist(Socket.CompatibleSocket)) continue;
			const FVector SocketLocation = Other->BuildingMesh->GetSocketLocation(Socket.CompatibleSocket);
			const float Distance = FVector::Dist(BuildTransform.GetLocation(), SocketLocation);
			if (Distance < SocketClosestDistance)
			{
				SocketClosestDistance = Distance;
				SnappedStructure = Other;
				UsedSocketName = Socket.SocketName;
				TargetSocketName = Socket.CompatibleSocket;
				MySocketTransform = BuildingMesh->GetSocketTransform(Socket.SocketName);
				OtherSocketTransform = Other->BuildingMesh->GetSocketTransform(Socket.CompatibleSocket);
				bSocketFaceEachOther = Socket.bFaceEachOther;
			}
		}
	}

	if (SocketClosestDistance < SnapRadius)
	{
		FTransform TargetSocket = OtherSocketTransform;
		if (bSocketFaceEachOther)
		{
			const FQuat Flip(TargetSocket.GetUnitAxis(EAxis::Z), PI);
			TargetSocket.SetRotation(Flip * TargetSocket.GetRotation());
		}
		const FTransform MyRelative = MySocketTransform.GetRelativeTransform(GetActorTransform());
		BuildTransform = MyRelative.Inverse() * TargetSocket;
		if (FMath::Abs(RotationOffset.Yaw) >= 180.f)
			BuildTransform.SetRotation(BuildTransform.GetRotation() * RotationOffset.Quaternion().GetNormalized());
	}
	else
	{
		BuildTransform.SetRotation(BuildTransform.GetRotation() * RotationOffset.Quaternion().GetNormalized());
		RotationOffset = FRotator::ZeroRotator;
		SnappedStructure = nullptr;
	}

	const bool bNewCanBuild = ValidateBuild(IsValid(SnappedStructure), SnappedStructure, UsedSocketName, TargetSocketName);
	if (!bBuildStateInitialized || bNewCanBuild != bCanBuild)
	{
		bBuildStateInitialized = true;
		bCanBuild = bNewCanBuild;
		UpdatePreview_BlueprintFunction(bCanBuild);
	}
}

void ABLDStructure::Trace()
{
	if (!BLDComponent || !BLDComponent->CameraRef) return;
	const FVector Start = BLDComponent->CameraRef->GetComponentLocation() + BLDComponent->CameraRef->GetForwardVector() * 350.f;
	const FVector End = BLDComponent->CameraRef->GetComponentLocation() + BLDComponent->CameraRef->GetForwardVector() * 750.f;
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		BuildTransform.SetLocation(Hit.ImpactPoint);
	else
		BuildTransform.SetLocation(End);
	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 5.f);
}

void ABLDStructure::RotationOfset(float ZOffset) { RotationOffset.Yaw += ZOffset; }
void ABLDStructure::ModifyBuildTransform_Implementation(FTransform& InOutTransform) {}
void ABLDStructure::UpdatePreview_BlueprintFunction_Implementation(bool CanBuild) {}
bool ABLDStructure::ValidateBuild_BlueprintSide_Implementation(ABLDStructure*, FName, FName) { return true; }
void ABLDStructure::OnStructureBuilt_Implementation() {}

bool ABLDStructure::ValidateBuild(bool HasSnapped, ABLDStructure* SnappedStructure, FName UsedSocketName, FName TargetSocketName)
{
	if (!HasSnapped && bShouldSnap) return false;
	const FCollisionShape Shape = FCollisionShape::MakeBox(ValidationBox->GetScaledBoxExtent());
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const FVector BoxLocation = BuildTransform.TransformPosition(ValidationBox->GetRelativeLocation());
	const FQuat BoxRotation = BuildTransform.GetRotation() * ValidationBox->GetRelativeRotation().Quaternion();
	TArray<FOverlapResult> Results;
	const bool bBlocked = GetWorld()->OverlapMultiByChannel(Results, BoxLocation, BoxRotation, ECC_WorldStatic, Shape, Params);
	if (bBlocked) return false;
	return ValidateBuild_BlueprintSide(SnappedStructure, UsedSocketName, TargetSocketName);
}

float ABLDStructure::GetHealth() const { return CurrentHealth; }
FName ABLDStructure::GetStructureName() const { return StructureName; }
void ABLDStructure::Add_DepleatHealth(float Health)
{
	if (!HasAuthority() || Health <= 0.f) return;
	CurrentHealth = FMath::Max(0.f, CurrentHealth - Health);
	if (CurrentHealth <= 0.f)
	{
		if (OwningBuildingBlock) OwningBuildingBlock->DestroyStructure(this);
		else Destroy();
	}
}
void ABLDStructure::UpgradeStructure() { UpgradeStructure(BuildingDataTable); }
