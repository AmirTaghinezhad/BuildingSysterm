#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "BLDComponet.generated.h"

class UDataTable;
class ABLDStructure;
class ABuildingBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLDUpdate, bool, CanBuild);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLDBuilt, bool, CanBuild);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetStructureUpdate, ABLDStructure*, Structure);

USTRUCT(Blueprintable, BlueprintType)
struct FBuildingInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) FName StructureName;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) FName DisplayName;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) TSubclassOf<ABLDStructure> StructureClass;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) UStaticMesh* StructureMesh = nullptr;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) FName RequiredResource;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) float RequiredAmount = 0.f;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) FName NextLevelStructureRowName;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) float MaxHealth = 100.f;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) float RepairCostPerHealth = 0.f;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) float RefundPercentage = 0.f;

	// Upkeep per in-game day = RequiredAmount * UpkeepPercentage / 100.
	// If UpkeepResource is None, RequiredResource is used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Upkeep")
	FName UpkeepResource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Upkeep", meta=(ClampMin="0.0"))
	float UpkeepPercentage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Upkeep", meta=(ClampMin="0.0", ClampMax="100.0"))
	float DecayPercentagePerDay = 10.f;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) UTexture2D* Texture = nullptr;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly) USoundBase* UISound = nullptr;
};

UENUM(BlueprintType)
enum class EStructureName : uint8
{
	Foundation_Wooden, Wall_Wooden, Roof_Wooden, Ceiling_Wooden, Stairs_Wooden, Tri_Roof_Wooden,
	Foundation_Stone, Wall_Stone, Roof_Stone, Ceiling_Stone, Stairs_Stone, Tri_Roof_Stone
};

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class BASEBUILDING_API UBLDComponet : public UActorComponent
{
	GENERATED_BODY()

public:
	UBLDComponet();

protected:
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable) void ToggleBuildMode(TSubclassOf<ABLDStructure> Structure);

	UFUNCTION(BlueprintCallable) void SpawnBuildGhost(TSubclassOf<ABLDStructure> Structure);

	UFUNCTION(BlueprintCallable) void BuildStructure();

	UFUNCTION(Server, Reliable)
	void ServerBuildStructure(TSubclassOf<ABLDStructure> StructureClass, FTransform RequestedTransform);

	UFUNCTION(Server, Reliable)
	void ServerUpgradeStructure(ABLDStructure* Structure);

	UFUNCTION(Server, Reliable)
	void ServerDemolishStructure(ABLDStructure* Structure);

	bool bDoOnce = true;
	FTimerHandle MainTraceCycleHandle;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere) class UCameraComponent* CameraRef;
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere) FTransform BuildTransform;
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere) bool IsBuildMoodOn;
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere) ACharacter* PlayerRefrence;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) ABLDStructure* BuildGhost;
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere) ABLDStructure* TargetStructure;

	UPROPERTY(BlueprintAssignable) FOnBLDUpdate OnBuildUpdate;
	UPROPERTY(BlueprintAssignable) FOnTargetStructureUpdate OnTargetStructureUpdate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(RequiredAssetDataTags="RowStructure=/Script/BaseBuilding.BuildingInfo"))
	UDataTable* StructureDataTable = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building Block", meta=(ClampMin="100.0"))
	float NewBuildingBlockRadius = 1500.f;

	UFUNCTION(BlueprintCallable) void AttemptRotation(float YawOffset);
	FVector GhostTrace();
	void MainTrace();
	void InitMainTrace();
	void StopMainTrace();

	UFUNCTION(BlueprintCallable) void StopGhost_Comp();
	UFUNCTION(BlueprintCallable) void UpgradeStructure(ABLDStructure* Structure);

	const FBuildingInfo* GetStructureData(FName StructureName) const;
	const FBuildingInfo* GetUpgradelStructureData(FName StructureName) const;
	bool UpdatePreview_BlueprintFunction(TArray<FBuildingInfo*> StructureInfo);

	UFUNCTION(BlueprintNativeEvent) void GetRefund();
	UFUNCTION(BlueprintCallable) void Demolish();
};