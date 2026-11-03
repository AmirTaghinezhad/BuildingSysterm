#include "BLDComponet.h"

#include "BLDStructure.h"
#include "BuildingBlock.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBLDComponet::UBLDComponet()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UBLDComponet::BeginPlay()
{
	Super::BeginPlay();
	PlayerRefrence = Cast<ACharacter>(GetOwner());
	if (!PlayerRefrence) UE_LOG(LogTemp, Error, TEXT("Player Reference is NULL!"));
}

void UBLDComponet::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UBLDComponet::AttemptRotation(float YawOffset)
{
	if (BuildGhost) BuildGhost->RotationOfset(YawOffset);
}

void UBLDComponet::ToggleBuildMode(TSubclassOf<ABLDStructure> Structure)
{
	if (!IsBuildMoodOn)
	{
		bDoOnce = false;
		IsBuildMoodOn = true;
		InitMainTrace();
	}
	else
	{
		if (BuildGhost)
		{
			BuildGhost->StopGhost();
			BuildGhost->Destroy();
			BuildGhost = nullptr;
		}
		IsBuildMoodOn = false;
		StopMainTrace();
	}
}

void UBLDComponet::SpawnBuildGhost(TSubclassOf<ABLDStructure> Structure)
{
	if (!IsBuildMoodOn || !Structure || !GetWorld()) return;
	if (BuildGhost)
	{
		BuildGhost->StopGhost();
		BuildGhost->Destroy();
		BuildGhost = nullptr;
	}
	StopMainTrace();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerRefrence;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BuildGhost = GetWorld()->SpawnActor<ABLDStructure>(Structure, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (BuildGhost)
	{
		BuildGhost->BLDComponent = this;
		BuildGhost->BuildingDataTable = StructureDataTable;
		BuildGhost->InitGhost();
	}
}

void UBLDComponet::MainTrace()
{
	if (!CameraRef || !GetWorld()) return;
	const FVector Start = CameraRef->GetComponentLocation() + CameraRef->GetForwardVector() * 350.f;
	const FVector End = CameraRef->GetComponentLocation() + CameraRef->GetForwardVector() * 600.f;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 5.f);
	ABLDStructure* HitStructure = bHit ? Cast<ABLDStructure>(HitResult.GetActor()) : nullptr;
	if (TargetStructure == HitStructure) return;
	TargetStructure = HitStructure;
	if (TargetStructure && TargetStructure->ValidationBox)
	{
		DrawDebugBox(GetWorld(), TargetStructure->GetActorLocation(), TargetStructure->ValidationBox->GetScaledBoxExtent(),
			TargetStructure->ValidationBox->GetComponentRotation().Quaternion(), FColor::Red, false, 1.f);
	}
	OnTargetStructureUpdate.Broadcast(TargetStructure);
}

FVector UBLDComponet::GhostTrace()
{
	if (!CameraRef || !GetWorld()) return FVector::ZeroVector;
	const FVector Start = CameraRef->GetComponentLocation() + CameraRef->GetForwardVector() * 350.f;
	const FVector End = CameraRef->GetComponentLocation() + CameraRef->GetForwardVector() * 750.f;
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 5.f);
	return bHit ? HitResult.ImpactPoint : End;
}

void UBLDComponet::BuildStructure()
{
	if (!IsBuildMoodOn || !BuildGhost || !BuildGhost->bCanBuild) return;
	const TSubclassOf<ABLDStructure> StructureClass = BuildGhost->GetClass();
	const FTransform RequestedTransform = BuildGhost->GetActorTransform();

	// The preview remains local. The server creates a separate replicated structure.
	ServerBuildStructure(StructureClass, RequestedTransform);

	BuildGhost->StopGhost();
	BuildGhost->Destroy();
	BuildGhost = nullptr;
	SpawnBuildGhost(StructureClass);
}

void UBLDComponet::ServerBuildStructure_Implementation(TSubclassOf<ABLDStructure> StructureClass, FTransform RequestedTransform)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !StructureClass || !GetWorld()) return;
	APlayerState* RequestingPlayer = PlayerRefrence ? PlayerRefrence->GetPlayerState() : nullptr;
	if (!RequestingPlayer || !PlayerRefrence || FVector::DistSquared(PlayerRefrence->GetActorLocation(), RequestedTransform.GetLocation()) > FMath::Square(1000.f)) return;
	if (!StructureDataTable) return;

	bool bClassAllowed = false;
	TArray<FBuildingInfo*> BuildingRows;
	StructureDataTable->GetAllRows<FBuildingInfo>(TEXT("ServerBuildStructure"), BuildingRows);
	for (const FBuildingInfo* Row : BuildingRows)
	{
		if (Row && Row->StructureClass == StructureClass) { bClassAllowed = true; break; }
	}
	if (!bClassAllowed) return;

	ABuildingBlock* ControllingBlock = ABuildingBlock::GetControllingBlockAtLocation(GetWorld(), RequestedTransform.GetLocation());
	// Outside all claims is allowed. Inside a claim, only its controlling block's owner may build.
	if (ControllingBlock && ControllingBlock->GetBuildingOwner() != RequestingPlayer) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABLDStructure* NewStructure = GetWorld()->SpawnActor<ABLDStructure>(StructureClass, RequestedTransform, SpawnParams);
	if (!NewStructure) return;

	NewStructure->BuildingDataTable = StructureDataTable;
	NewStructure->BLDComponent = this;
	if (!NewStructure->ValidateServerPlacement())
	{
		NewStructure->Destroy();
		return;
	}

	NewStructure->InitializeBuiltStructure(StructureDataTable, RequestingPlayer, ControllingBlock);
	NewStructure->OnStructureBuilt();
}

void UBLDComponet::InitMainTrace()
{
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(MainTraceCycleHandle, this, &UBLDComponet::MainTrace, 0.1f, true);
}

void UBLDComponet::StopMainTrace()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(MainTraceCycleHandle);
}

void UBLDComponet::UpgradeStructure(ABLDStructure* Structure)
{
	if (!IsBuildMoodOn || !Structure || !Structure->bIsbuilt) return;
	ServerUpgradeStructure(Structure);
}

void UBLDComponet::ServerUpgradeStructure_Implementation(ABLDStructure* Structure)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Structure)) return;
	APlayerState* RequestingPlayer = PlayerRefrence ? PlayerRefrence->GetPlayerState() : nullptr;
	if (!RequestingPlayer || Structure->GetBuildingOwner() != RequestingPlayer || !PlayerRefrence ||
		FVector::DistSquared(PlayerRefrence->GetActorLocation(), Structure->GetActorLocation()) > FMath::Square(600.f)) return;
	Structure->UpgradeStructure(StructureDataTable);
}

const FBuildingInfo* UBLDComponet::GetStructureData(FName StructureName) const
{
	return StructureDataTable ? StructureDataTable->FindRow<FBuildingInfo>(StructureName, TEXT("GetStructureData")) : nullptr;
}

const FBuildingInfo* UBLDComponet::GetUpgradelStructureData(FName StructureName) const
{
	const FBuildingInfo* Current = GetStructureData(StructureName);
	return Current ? GetStructureData(Current->NextLevelStructureRowName) : nullptr;
}

bool UBLDComponet::UpdatePreview_BlueprintFunction(TArray<FBuildingInfo*> StructureInfo)
{
	return false;
}

void UBLDComponet::Demolish()
{
	if (!TargetStructure) return;
	ServerDemolishStructure(TargetStructure);
	TargetStructure = nullptr;
	OnTargetStructureUpdate.Broadcast(nullptr);
}

void UBLDComponet::ServerDemolishStructure_Implementation(ABLDStructure* Structure)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Structure)) return;
	APlayerState* RequestingPlayer = PlayerRefrence ? PlayerRefrence->GetPlayerState() : nullptr;
	if (!RequestingPlayer || Structure->GetBuildingOwner() != RequestingPlayer || !PlayerRefrence ||
		FVector::DistSquared(PlayerRefrence->GetActorLocation(), Structure->GetActorLocation()) > FMath::Square(600.f)) return;

	GetRefund();
	ABuildingBlock* Block = Structure->GetOwningBuildingBlock();
	Structure->Destroy();
	if (Block) Block->RecalculateUpkeepRequirements();
}

void UBLDComponet::GetRefund_Implementation() {}

void UBLDComponet::StopGhost_Comp()
{
	if (BuildGhost) BuildGhost->StopGhost();
}
