#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BLDStructure.generated.h"

enum class EStructureName : uint8;
enum class EStructureType : uint8;
class UBoxComponent;
class UBLDComponet;
class UDataTable;
class ABuildingBlock;
class APlayerState;

USTRUCT(BlueprintType)
struct FSnapSocket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName SocketName;

	UPROPERTY(EditAnywhere)
	FName CompatibleSocket;

	UPROPERTY(EditAnywhere)
	bool bFaceEachOther = false;
};

UCLASS()
class BASEBUILDING_API ABLDStructure : public AActor
{
	GENERATED_BODY()

public:
	ABLDStructure();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateBuildPreview();

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	UStaticMeshComponent* BuildingMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UBoxComponent* ValidationBox;

	UPROPERTY(BlueprintReadWrite, Replicated)
	bool bIsbuilt = false;

	UPROPERTY(BlueprintReadWrite, Replicated)
	bool bCanBuild = false;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	bool bShouldSnap = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_StructureName)
	FName StructureName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building")
	TObjectPtr<UDataTable> BuildingDataTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Building")
	TObjectPtr<APlayerState> BuildingOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Building")
	TObjectPtr<ABuildingBlock> OwningBuildingBlock;

	UFUNCTION(BlueprintPure, Category="Building")
	ABuildingBlock* GetOwningBuildingBlock() const { return OwningBuildingBlock; }

	UFUNCTION(BlueprintPure, Category="Building")
	APlayerState* GetBuildingOwner() const { return BuildingOwner; }

	UFUNCTION(BlueprintPure, Category="Building")
	bool IsBuilt() const { return bIsbuilt; }

	// Server-only initialization for a newly placed, replicated structure.
	void InitializeBuiltStructure(UDataTable* InDataTable, APlayerState* InOwner, ABuildingBlock* InBlock);\n\tbool ValidateServerPlacement();

	// Upkeep is derived from the current structure row. A blank upkeep resource falls back to construction resource.
	bool GetUpkeepRequirement(FName& OutResource, float& OutAmount) const;
	void ApplyUpkeepDecay(float PercentageOfMaxHealth);
	void UpgradeStructure(UDataTable* DataTable);

protected:
	virtual void BeginPlay() override;
	FTimerHandle CycleHandle;
	bool bBuildStateInitialized = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FSnapSocket> SnapSockets;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	bool bCanFlip = true;

private:
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing=OnRep_CurrentHealth)
	float CurrentHealth = 0.f;

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_StructureName();

	void ApplyStructureData();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UBLDComponet* BLDComponent;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FTransform BuildTransform;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	float SnapRadius = 200.f;

	void InitGhost();
	void StopGhost();
	void BuildCycle();
	void Trace();
	void RotationOfset(float ZOffset);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ModifyBuildTransform(UPARAM(ref) FTransform& InOutTransform);

	UFUNCTION(BlueprintNativeEvent)
	void UpdatePreview_BlueprintFunction(bool CanBuild);

	bool ValidateBuild(bool HasSnapped, ABLDStructure* SnappedStructure, FName UsedSocketName, FName TargetSocketName);

	UFUNCTION(BlueprintNativeEvent)
	void OnStructureBuilt();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Building")
	bool ValidateBuild_BlueprintSide(ABLDStructure* SnappedStructure, FName UsedSocketName, FName TargetSocketName);

	float GetHealth() const;
	FName GetStructureName() const;
	void Add_DepleatHealth(float Health);
	void UpgradeStructure();
};