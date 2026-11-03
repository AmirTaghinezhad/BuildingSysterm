#include "BuildingBlock.h"

#include "BLDStructure.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

int64 ABuildingBlock::NextCreationOrder = 1;

ABuildingBlock::ABuildingBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	TerritorySphere = CreateDefaultSubobject<USphereComponent>(TEXT("TerritorySphere"));
	TerritorySphere->SetupAttachment(RootComponent);
	TerritorySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TerritorySphere->SetCollisionObjectType(ECC_WorldDynamic);
	TerritorySphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TerritorySphere->SetGenerateOverlapEvents(false);
	TerritorySphere->SetHiddenInGame(true);
	TerritorySphere->SetSphereRadius(TerritoryRadius);
}

void ABuildingBlock::BeginPlay()
{
	Super::BeginPlay();
	RefreshTerritoryRadius();

	if (HasAuthority())
	{
		if (CreationOrder <= 0)
		{
			CreationOrder = NextCreationOrder++;
		}
		LastUpkeepGameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		GetWorldTimerManager().SetTimer(UpkeepTimerHandle, this, &ABuildingBlock::CheckUpkeepClock, 1.f, true);
		RecalculateUpkeepRequirements();
	}
}

void ABuildingBlock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UpkeepTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ABuildingBlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABuildingBlock, TerritoryRadius);
	DOREPLIFETIME(ABuildingBlock, BuildingOwner);
	DOREPLIFETIME(ABuildingBlock, CreationOrder);
	DOREPLIFETIME(ABuildingBlock, RequiredUpkeepPerDay);
	DOREPLIFETIME(ABuildingBlock, StoredMaterials);
	DOREPLIFETIME(ABuildingBlock, LastUpkeepGameTime);
	DOREPLIFETIME(ABuildingBlock, bUpkeepPaidForCurrentPeriod);
}

void ABuildingBlock::InitializeBuildingBlock(APlayerState* InOwner, float InRadius)
{
	if (!HasAuthority()) return;
	BuildingOwner = InOwner;
	if (InRadius > 0.f) TerritoryRadius = InRadius;
	if (CreationOrder <= 0) CreationOrder = NextCreationOrder++;
	RefreshTerritoryRadius();
	ForceNetUpdate();
}

void ABuildingBlock::RefreshTerritoryRadius()
{
	if (TerritorySphere) TerritorySphere->SetSphereRadius(TerritoryRadius);
}

bool ABuildingBlock::IsLocationInsideRadius(const FVector& Location) const
{
	const FVector Delta = Location - GetActorLocation();
	return Delta.SizeSquared() <= FMath::Square(TerritoryRadius);
}

ABuildingBlock* ABuildingBlock::GetControllingBlockAtLocation(UWorld* World, const FVector& Location)
{
	if (!World) return nullptr;
	ABuildingBlock* OldestBlock = nullptr;
	for (TActorIterator<ABuildingBlock> It(World); It; ++It)
	{
		ABuildingBlock* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->IsLocationInsideRadius(Location)) continue;
		if (!OldestBlock || Candidate->CreationOrder < OldestBlock->CreationOrder)
		{
			OldestBlock = Candidate;
		}
	}
	return OldestBlock;
}

bool ABuildingBlock::IsLocationOwnedBy(UWorld* World, const FVector& Location, const APlayerState* Player)
{
	if (!Player) return false;
	const ABuildingBlock* Controller = GetControllingBlockAtLocation(World, Location);
	return Controller && Controller->BuildingOwner == Player;
}

bool ABuildingBlock::CanPlaceBlockAtLocation(UWorld* World, const FVector& Location)
{
	// Overlapping blocks are valid. Oldest-block priority clips the newer block's effective territory.
	return World != nullptr;
}

void ABuildingBlock::RecalculateUpkeepRequirements()
{
	if (!HasAuthority() || !GetWorld()) return;

	RequiredUpkeepPerDay.Reset();
	for (TActorIterator<ABLDStructure> It(GetWorld()); It; ++It)
	{
		ABLDStructure* Structure = *It;
		if (!IsValid(Structure) || Structure->GetOwningBuildingBlock() != this || !Structure->IsBuilt()) continue;

		FName Resource = NAME_None;
		float Amount = 0.f;
		if (Structure->GetUpkeepRequirement(Resource, Amount) && !Resource.IsNone() && Amount > 0.f)
		{
			RequiredUpkeepPerDay.FindOrAdd(Resource) += Amount;
		}
	}
	ForceNetUpdate();
}

bool ABuildingBlock::AddStoredMaterial(FName Resource, float Amount)
{
	if (!HasAuthority() || Resource.IsNone() || Amount <= 0.f) return false;
	StoredMaterials.FindOrAdd(Resource) += Amount;
	ForceNetUpdate();
	return true;
}

float ABuildingBlock::GetStoredMaterial(FName Resource) const
{
	if (const float* Amount = StoredMaterials.Find(Resource)) return *Amount;
	return 0.f;
}

void ABuildingBlock::CheckUpkeepClock()
{
	if (!HasAuthority() || !GetWorld() || GameSecondsPerDay <= 0.f) return;
	const float CurrentGameTime = GetWorld()->GetTimeSeconds();
	if (CurrentGameTime - LastUpkeepGameTime < GameSecondsPerDay) return;

	ProcessUpkeepDay();
	LastUpkeepGameTime = CurrentGameTime;
	ForceNetUpdate();
}

void ABuildingBlock::ProcessUpkeepDay()
{
	if (!HasAuthority()) return;
	RecalculateUpkeepRequirements();

	bool bCanPay = true;
	for (const TPair<FName, float>& Requirement : RequiredUpkeepPerDay)
	{
		if (GetStoredMaterial(Requirement.Key) + KINDA_SMALL_NUMBER < Requirement.Value)
		{
			bCanPay = false;
			break;
		}
	}

	if (bCanPay)
	{
		for (const TPair<FName, float>& Requirement : RequiredUpkeepPerDay)
		{
			float& Stored = StoredMaterials.FindOrAdd(Requirement.Key);
			Stored = FMath::Max(0.f, Stored - Requirement.Value);
		}
		bUpkeepPaidForCurrentPeriod = true;
	}
	else
	{
		bUpkeepPaidForCurrentPeriod = false;
		ApplyDecayToStructures();
	}
	ForceNetUpdate();
}

void ABuildingBlock::ApplyDecayToStructures()
{
	if (!HasAuthority() || !GetWorld()) return;
	TArray<ABLDStructure*> StructuresToDecay;
	for (TActorIterator<ABLDStructure> It(GetWorld()); It; ++It)
	{
		ABLDStructure* Structure = *It;
		if (IsValid(Structure) && Structure->GetOwningBuildingBlock() == this && Structure->IsBuilt())
		{
			StructuresToDecay.Add(Structure);
		}
	}

	for (ABLDStructure* Structure : StructuresToDecay)
	{
		Structure->ApplyUpkeepDecay(-1.f);
	}
}

void ABuildingBlock::DestroyStructure(ABLDStructure* Structure)
{
	if (!HasAuthority() || !IsValid(Structure) || Structure->GetOwningBuildingBlock() != this) return;
	Structure->Destroy();
	RecalculateUpkeepRequirements();
}

void ABuildingBlock::OnRep_UpkeepData()
{
	// UI-facing Blueprint can observe the replicated maps and payment state.
}