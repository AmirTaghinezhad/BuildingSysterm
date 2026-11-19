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
#include "Kismet/GameplayStatics.h"

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
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		FindBuildingBlockWhenReady();
	}
}

ABuildingBlock* UBLDComponet::GetControllingBuildingBlockAtLocation(
	const FVector& Location
) const
{
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error,
			TEXT("BLDComponent: GetWorld() returned NULL"));

		return nullptr;
	}

	TArray<AActor*> FoundActors;

	// Use this component as the world context.
	UGameplayStatics::GetAllActorsOfClass(
		this,
		ABuildingBlock::StaticClass(),
		FoundActors
	);

	UE_LOG(LogTemp, Warning,
		TEXT("BLDComponent: World=%s | NetMode=%d | Found=%d | Location=%s"),
		*GetWorld()->GetName(),
		static_cast<int32>(GetWorld()->GetNetMode()),
		FoundActors.Num(),
		*Location.ToString()
	);

	ABuildingBlock* OldestBlock = nullptr;

	for (AActor* Actor : FoundActors)
	{
		ABuildingBlock* Candidate = Cast<ABuildingBlock>(Actor);

		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed())
		{
			continue;
		}

		if (!Candidate->IsLocationInsideRadius(Location))
		{
			continue;
		}

		if (!OldestBlock ||
			Candidate->GetCreationOrder() < OldestBlock->GetCreationOrder())
		{
			OldestBlock = Candidate;
		}
	}

	if (OldestBlock)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BLDComponent: Controlling block found: %s"),
			*OldestBlock->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BLDComponent: No controlling block found."));
	}

	return OldestBlock;
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

	ABuildingBlock* ControllingBlock =GetControllingBuildingBlockAtLocation(RequestedTransform.GetLocation());
	// Outside all claims is allowed. Inside a claim, only its controlling block's owner may build.
	if (ControllingBlock && ControllingBlock->GetBuildingOwner() != RequestingPlayer) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABLDStructure* NewStructure = GetWorld()->SpawnActor<ABLDStructure>(StructureClass, RequestedTransform, SpawnParams);
	if (!NewStructure) return;

	NewStructure->BuildingDataTable = StructureDataTable;
	NewStructure->BuildingOwner = RequestingPlayer;
	NewStructure->BLDComponent = this;
	if (!NewStructure->ValidateServerPlacement())
	{
		NewStructure->Destroy();
		return;
	}
	if (ControllingBlock)
	{
		float rad = ControllingBlock->GetTerritoryRadius();
		UE_LOG(LogTemp, Log, TEXT("build block rad is %f"), rad);
	}
	UE_LOG(LogTemp, Log, TEXT("Structure constructions called"));
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
	if (Block) Block->DestroyStructure(Structure);
	else Structure->Destroy();
}

void UBLDComponet::GetRefund_Implementation() {}

void UBLDComponet::StopGhost_Comp()
{
	if (BuildGhost) BuildGhost->StopGhost();
}

void UBLDComponet::FindBuildingBlockWhenReady()
{
	if (!GetWorld())
	{
		return;
	}

	TArray<AActor*> FoundActors;

	UGameplayStatics::GetAllActorsOfClass(
		this,
		ABuildingBlock::StaticClass(),
		FoundActors
	);

	UE_LOG(LogTemp, Warning,
		TEXT("BUILDING BLOCK SEARCH: NetMode=%d | Found=%d"),
		static_cast<int32>(GetWorld()->GetNetMode()),
		FoundActors.Num()
	);

	if (FoundActors.Num() == 0)
	{
		// Retry on the next tick.
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			this,
			&UBLDComponet::FindBuildingBlockWhenReady
		);

		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("BUILDING BLOCK SEARCH SUCCESS"));
}