#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBlock.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APlayerState;
class ABLDStructure;

UCLASS(Blueprintable)
class BASEBUILDING_API ABuildingBlock : public AActor
{
	GENERATED_BODY()

public:
	ABuildingBlock();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Must be called by the server after spawning the block.
	UFUNCTION(BlueprintCallable, Category="Building Block")
	void InitializeBuildingBlock(APlayerState* InOwner, float InRadius = -1.f);

	UFUNCTION(BlueprintPure, Category="Building Block")
	APlayerState* GetBuildingOwner() const { return BuildingOwner; }

	UFUNCTION(BlueprintPure, Category="Building Block")
	float GetTerritoryRadius() const { return TerritoryRadius; }

	UFUNCTION(BlueprintPure, Category="Building Block")
	int64 GetCreationOrder() const { return CreationOrder; }

	UFUNCTION(BlueprintPure, Category="Building Block")
	bool IsLocationInsideRadius(const FVector& Location) const;
	UFUNCTION(BlueprintPure, Category="Building Block")
	TArray<ABuildingBlock*> GetOverlappingBuildingBlocks() const;

	// Resolves overlapping claims using the oldest block that contains the location.
	static ABuildingBlock* GetControllingBlockAtLocation(UWorld* World, const FVector& Location);
	static bool IsLocationOwnedBy(UWorld* World, const FVector& Location, const APlayerState* Player);

	// One period represents one in-game day. The value is measured in game-world seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upkeep", meta=(ClampMin="1.0"))
	float GameSecondsPerDay = 86400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Upkeep", meta=(ClampMin="0.0", ClampMax="100.0"))
	float DefaultDecayPercentagePerDay = 10.f;

	// Rebuilds the material totals from all structures assigned to this block.
	void RecalculateUpkeepRequirements();

	// Executes one upkeep period. Payment is all-or-nothing across the block.
	void ProcessUpkeepDay();

	// Core storage API. Inventory integration will call this later, on the server.
	bool AddStoredMaterial(FName Resource, float Amount);
	float GetStoredMaterial(FName Resource) const;
	const TMap<FName, float>& GetRequiredUpkeepPerDay() const { return RequiredUpkeepPerDay; }
	const TMap<FName, float>& GetStoredMaterials() const { return StoredMaterials; }

	// Explicit lifecycle operations used by the upkeep system.
	void ApplyDecayToStructures();
	void DestroyStructure(ABLDStructure* Structure);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building Block")
	TObjectPtr<UStaticMeshComponent> BlockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building Block")
	TObjectPtr<USphereComponent> TerritorySphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_TerritoryRadius, Category="Building Block", meta=(ClampMin="100.0"))
	float TerritoryRadius = 1500.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Building Block")
	TObjectPtr<APlayerState> BuildingOwner;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Building Block")
	int64 CreationOrder = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_UpkeepData, Category="Upkeep")
	TMap<FName, float> RequiredUpkeepPerDay;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_UpkeepData, Category="Upkeep")
	TMap<FName, float> StoredMaterials;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Upkeep")
	float LastUpkeepGameTime = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Upkeep")
	bool bUpkeepPaidForCurrentPeriod = true;

	UFUNCTION()
	void OnRep_UpkeepData();

	UFUNCTION()
	void OnRep_TerritoryRadius();

private:
	FTimerHandle UpkeepTimerHandle;
	void CheckUpkeepClock();
	void RefreshTerritoryRadius();
	void ClaimUnassignedStructures();
	static int64 NextCreationOrder;
};