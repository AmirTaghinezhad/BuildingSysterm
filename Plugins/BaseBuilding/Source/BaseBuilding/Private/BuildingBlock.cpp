#include "BuildingBlock.h"

#include "BLDStructure.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
//#include "Kismet/GameplayStatics.h"
#include "Kismet/GameplayStatics.h"

int64 ABuildingBlock::NextCreationOrder = 1;

ABuildingBlock::ABuildingBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	BlockMesh->SetupAttachment(RootComponent);
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
	UE_LOG(LogTemp, Warning,
	   TEXT("BUILDING BLOCK INSTANCE:"
			" Name=%s"
			" | Class=%s"
			" | NetMode=%d"
			" | Authority=%d"
			" | LocalRole=%d"
			" | RemoteRole=%d"
			" | Replicates=%d"
			" | Level=%s"
			" | World=%s"),

	   *GetName(),
	   *GetClass()->GetName(),
	   static_cast<int32>(GetNetMode()),
	   HasAuthority(),
	   static_cast<int32>(GetLocalRole()),
	   static_cast<int32>(GetRemoteRole()),
	   GetIsReplicated(),
	   GetLevel() ? *GetLevel()->GetName() : TEXT("NULL"),
	   *GetWorld()->GetName()
   );

	int32 TotalActors = 0;
	int32 BuildingBlocksFound = 0;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		TotalActors++;

		if (Actor->IsA(ABuildingBlock::StaticClass()))
		{
			BuildingBlocksFound++;

			UE_LOG(LogTemp, Warning,
				TEXT("BLOCK DISCOVERY: Found %s | Class=%s"),
				*Actor->GetName(),
				*Actor->GetClass()->GetName());
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("BLOCK DISCOVERY SUMMARY: Actors=%d | BuildingBlocks=%d"),
		TotalActors,
		BuildingBlocksFound);
	
	TArray<AActor*> FoundActors;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		ABuildingBlock::StaticClass(),
		FoundActors);

	UE_LOG(LogTemp, Warning,
			TEXT("BLDComponent: World=%s | NetMode=%d | Found=%d "),
			*GetWorld()->GetName(),
			static_cast<int32>(GetWorld()->GetNetMode()),
			FoundActors.Num()
		);
	
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
	
	UE_LOG(LogTemp, Warning,
	TEXT("BUILDING BLOCK READY: %s | World=%s | NetMode=%d | Authority=%d"),
	*GetName(),
	*GetWorld()->GetName(),
	static_cast<int32>(GetWorld()->GetNetMode()),
	HasAuthority()
);
}

void ABuildingBlock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UpkeepTimerHandle);
	if (HasAuthority() && GetWorld())
	{
		for (TActorIterator<ABLDStructure> It(GetWorld()); It; ++It)
		{
			ABLDStructure* Structure = *It;
			if (IsValid(Structure) && Structure->GetOwningBuildingBlock() == this)
			{
				Structure->SetOwningBuildingBlock(nullptr);
			}
		}
		for (TActorIterator<ABuildingBlock> It(GetWorld()); It; ++It)
		{
			ABuildingBlock* OtherBlock = *It;
			if (!IsValid(OtherBlock) || OtherBlock == this || OtherBlock->IsActorBeingDestroyed()) continue;
			OtherBlock->ClaimUnassignedStructures();
			OtherBlock->RecalculateUpkeepRequirements();
		}
	}
	UE_LOG(LogTemp, Warning,
	TEXT("BUILDING BLOCK END PLAY: %s | World=%s | NetMode=%d"),
	*GetName(),
	*GetWorld()->GetName(),
	static_cast<int32>(GetWorld()->GetNetMode())
);
	
	Super::EndPlay(EndPlayReason);
}

void ABuildingBlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABuildingBlock, TerritoryRadius);
	DOREPLIFETIME(ABuildingBlock, BuildingOwner);
	DOREPLIFETIME(ABuildingBlock, CreationOrder);
	DOREPLIFETIME(ABuildingBlock, ReplicatedRequiredUpkeep);
	DOREPLIFETIME(ABuildingBlock, ReplicatedStoredMaterials);
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
	ClaimUnassignedStructures();
	RecalculateUpkeepRequirements();
	ForceNetUpdate();
}

void ABuildingBlock::RefreshTerritoryRadius()
{
	if (TerritorySphere) TerritorySphere->SetSphereRadius(TerritoryRadius);
}

void ABuildingBlock::ClaimUnassignedStructures()
{
	if (!HasAuthority() || !GetWorld() || !BuildingOwner) return;
	for (TActorIterator<ABLDStructure> It(GetWorld()); It; ++It)
	{
		ABLDStructure* Structure = *It;
		if (!IsValid(Structure) || Structure->IsActorBeingDestroyed() || !Structure->IsBuilt() || Structure->GetOwningBuildingBlock()) continue;
		if (Structure->GetBuildingOwner() != BuildingOwner) continue;
		if (GetControllingBlockAtLocation(GetWorld(), Structure->GetActorLocation()) != this) continue;
		Structure->SetOwningBuildingBlock(this);
	}
}

bool ABuildingBlock::IsLocationInsideRadius(const FVector& Location) const
{
	const FVector2D Delta(Location.X - GetActorLocation().X, Location.Y - GetActorLocation().Y);
	return Delta.SizeSquared() <= FMath::Square(TerritoryRadius);
}

TArray<ABuildingBlock*> ABuildingBlock::GetOverlappingBuildingBlocks() const
{
	TArray<ABuildingBlock*> Result;
	if (!GetWorld()) return Result;
	for (TActorIterator<ABuildingBlock> It(GetWorld()); It; ++It)
	{
		ABuildingBlock* Other = *It;
		if (!IsValid(Other) || Other == this) continue;
		const FVector2D Delta(Other->GetActorLocation().X - GetActorLocation().X,
			Other->GetActorLocation().Y - GetActorLocation().Y);
		const float CombinedRadius = TerritoryRadius + Other->TerritoryRadius;
		if (Delta.SizeSquared() <= FMath::Square(CombinedRadius))
		{
			Result.Add(Other);
		}
	}
	return Result;
}

ABuildingBlock* ABuildingBlock::GetControllingBlockAtLocation(
	UWorld* World,
	const FVector& Location)
{
	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("Block: world not valid"));
		return nullptr;
	}

	TArray<AActor*> FoundActors;

	UGameplayStatics::GetAllActorsOfClass(
		World,
		ABuildingBlock::StaticClass(),
		FoundActors);
	UE_LOG(LogTemp, Warning,
			TEXT("getcontrollingblock: GET ALL ACTORS OF CLASS: Found=%d"),
			FoundActors.Num());
	ABuildingBlock* OldestBlock = nullptr;

	for (AActor* Actor : FoundActors)
	{UE_LOG(LogTemp, Log, TEXT("Block: Inside founcactors loop"));
	
		ABuildingBlock* Candidate = Cast<ABuildingBlock>(Actor);

		if (!IsValid(Candidate) ||
			Candidate->IsActorBeingDestroyed() ||
			!Candidate->IsLocationInsideRadius(Location))
		{
			continue;
		}

		if (!OldestBlock ||
			Candidate->GetCreationOrder() < OldestBlock->GetCreationOrder())
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

void ABuildingBlock::RecalculateUpkeepRequirements()
{
	if (!HasAuthority() || !GetWorld()) return;
	UE_LOG(LogTemp, Log, TEXT("RecalculateUpkeepRequirements entered"));
	RequiredUpkeepPerDay.Reset();
	for (TActorIterator<ABLDStructure> It(GetWorld()); It; ++It)
	{
		ABLDStructure* Structure = *It;
		if (!IsValid(Structure))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid structure"));
		}
		if (!IsValid(Structure->GetOwningBuildingBlock()))
		{
			UE_LOG(LogTemp, Log, TEXT("Owning building block is valid"));
		}
		if (Structure->GetOwningBuildingBlock() != this)
		{
			UE_LOG(LogTemp, Log, TEXT("Building block not this(valid)"));
		}
		if (!IsValid(Structure) || Structure->IsActorBeingDestroyed() || Structure->GetOwningBuildingBlock() != this || !Structure->IsBuilt()) continue;
		UE_LOG(LogTemp, Log, TEXT("recalculat: passed first condition"));
		FName Resource = NAME_None;
		float Amount = 0.f;
		if (Structure->GetUpkeepRequirement(Resource, Amount) && !Resource.IsNone() && Amount > 0.f)
		{
			UE_LOG(LogTemp, Log, TEXT("Calculated the req resource"));
			RequiredUpkeepPerDay.FindOrAdd(Resource) += Amount;
		}
	}
	SyncReplicatedUpkeepData();
	//ForceNetUpdate();
	
}

bool ABuildingBlock::AddStoredMaterial(FName Resource, float Amount)
{
	if (!HasAuthority() || Resource.IsNone() || Amount <= 0.f) return false;
	StoredMaterials.FindOrAdd(Resource) += Amount;
	SyncReplicatedUpkeepData();
	//ForceNetUpdate();
	return true;
}

float ABuildingBlock::GetStoredMaterial(FName Resource) const
{
	if (const float* Amount = StoredMaterials.Find(Resource)) return *Amount;
	return 0.f;
}

void ABuildingBlock::CheckUpkeepClock()
{UE_LOG(LogTemp, Log, TEXT("CheckUpkeepClock"));

	if (!HasAuthority() || !GetWorld() || GameSecondsPerDay <= 0.f) return;
	const float CurrentGameTime = GetWorld()->GetTimeSeconds();
	while (CurrentGameTime - LastUpkeepGameTime >= GameSecondsPerDay)
	{
		ProcessUpkeepDay();
		LastUpkeepGameTime += GameSecondsPerDay;
	}
	ForceNetUpdate();
}

void ABuildingBlock::ProcessUpkeepDay()
{
	if (!HasAuthority()) return;
	RecalculateUpkeepRequirements();
	UE_LOG(LogTemp, Log, TEXT("ProcessUpkeepDay"));
	bool bCanPay = true;
	for (const TPair<FName, float>& Requirement : RequiredUpkeepPerDay)
	{
		UE_LOG(LogTemp, Log, TEXT("inside upkeep checl loop"));
		if (GetStoredMaterial(Requirement.Key) + KINDA_SMALL_NUMBER < Requirement.Value)
		{
			UE_LOG(LogTemp, Log, TEXT("set cant pay"));
			bCanPay = false;
			break;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Eghiter can pay or is invalid"));
		}
	}

	if (bCanPay)
	{
		UE_LOG(LogTemp, Log, TEXT("can pay"));
		for (const TPair<FName, float>& Requirement : RequiredUpkeepPerDay)
		{
			float& Stored = StoredMaterials.FindOrAdd(Requirement.Key);
			Stored = FMath::Max(0.f, Stored - Requirement.Value);
		}
		bUpkeepPaidForCurrentPeriod = true;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("cant pay"));
		bUpkeepPaidForCurrentPeriod = false;
		ApplyDecayToStructures();
	}
	SyncReplicatedUpkeepData();
	//ForceNetUpdate();
}

void ABuildingBlock::ApplyDecayToStructures()
{
	UE_LOG(LogTemp, Log, TEXT("Apply decay reached"));
	if (!HasAuthority() || !GetWorld()) return;
	TArray<ABLDStructure*> StructuresToDecay;
	for (TActorIterator<ABLDStructure> It(GetWorld()); It; ++It)
	{
		ABLDStructure* Structure = *It;
		if (IsValid(Structure) && !Structure->IsActorBeingDestroyed() && Structure->GetOwningBuildingBlock() == this && Structure->IsBuilt())
		{
			StructuresToDecay.Add(Structure);
		}
	}

	for (ABLDStructure* Structure : StructuresToDecay)
	{
		Structure->ApplyUpkeepDecay(Structure->GetDecayPercentagePerDay(DefaultDecayPercentagePerDay));
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
	RequiredUpkeepPerDay.Reset();
	StoredMaterials.Reset();

	for (const FBuildingBlockResourceEntry& Entry : ReplicatedRequiredUpkeep)
	{
		RequiredUpkeepPerDay.Add(Entry.Resource, Entry.Amount);
	}

	for (const FBuildingBlockResourceEntry& Entry : ReplicatedStoredMaterials)
	{
		StoredMaterials.Add(Entry.Resource, Entry.Amount);
	}
}

void ABuildingBlock::OnRep_TerritoryRadius()
{
	RefreshTerritoryRadius();
}

void ABuildingBlock::SyncReplicatedUpkeepData()
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedRequiredUpkeep.Reset();
	ReplicatedStoredMaterials.Reset();

	for (const TPair<FName, float>& Entry : RequiredUpkeepPerDay)
	{
		FBuildingBlockResourceEntry& NewEntry =
			ReplicatedRequiredUpkeep.AddDefaulted_GetRef();

		NewEntry.Resource = Entry.Key;
		NewEntry.Amount = Entry.Value;
	}

	for (const TPair<FName, float>& Entry : StoredMaterials)
	{
		FBuildingBlockResourceEntry& NewEntry =
			ReplicatedStoredMaterials.AddDefaulted_GetRef();

		NewEntry.Resource = Entry.Key;
		NewEntry.Amount = Entry.Value;
	}

	ForceNetUpdate();
}