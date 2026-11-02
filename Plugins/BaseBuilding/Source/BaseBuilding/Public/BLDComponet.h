// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "BLDComponet.generated.h"

class UDataTable;
class ABLDStructure;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLDUpdate, bool, CanBuild);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLDBuilt, bool, CanBuild);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetStructureUpdate, ABLDStructure*, Structure);

USTRUCT(Blueprintable, BlueprintType)
struct FBuildingInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	FName StructureName;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	FName DisplayName;
	
	UPROPERTY(BLueprintReadWrite, EditDefaultsOnly)
	TSubclassOf<ABLDStructure> StructureClass;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	UStaticMesh* StructureMesh;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	FName RequiredResource;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	float RequiredAmount;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	FName NextLevelStructureRowName;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	float MaxHealth;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	float RepairCostPerHealth;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	float RefundPercentage;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	UTexture2D* Texture;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly)
	USoundBase* UISound;
	
};

UENUM(BlueprintType)
enum class EStructureName : uint8
{
	Foundation_Wooden,
	Wall_Wooden,
	Roof_Wooden,
	Ceiling_Wooden,
	Stairs_Wooden,
	Tri_Roof_Wooden,
	Foundation_Stone,
	Wall_Stone,
	Roof_Stone,
	Ceiling_Stone,
	Stairs_Stone,
	Tri_Roof_Stone
	
};
UCLASS(ClassGroup=(Custom),Blueprintable, meta=(BlueprintSpawnableComponent))
class BASEBUILDING_API UBLDComponet : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBLDComponet();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void ToggleBuildMode(TSubclassOf<ABLDStructure> Structure);
	void BuildCycle();
	
	UFUNCTION(BlueprintCallable)
	void SpawnBuildGhost(TSubclassOf<ABLDStructure> Structure);
	void UpdateBuildPreview();
	
	UFUNCTION(BlueprintCallable)
	void BuildStructure();

	bool bDoOnce = true;

	FTimerHandle MainTraceCycleHandle;
	
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	class UCameraComponent* CameraRef;

	//UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	//UStaticMeshComponent* BuildGhost;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FTransform BuildTransform;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	bool IsBuildMoodOn;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	ACharacter* PlayerRefrence;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	ABLDStructure* BuildGhost;
	
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	ABLDStructure* TargetStructure;
	
	UPROPERTY(BlueprintAssignable)
	FOnBLDUpdate OnBuildUpdate;
	
	UPROPERTY(BlueprintAssignable)
	FOnTargetStructureUpdate OnTargetStructureUpdate;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,   meta = (RequiredAssetDataTags = "RowStructure=/Script/BaseBuilding.BuildingInfo"))
	UDataTable* StructureDataTable = nullptr;
	
	UFUNCTION(BlueprintCallable)
	void AttemptRotation(float YawOffset);
	FVector GhostTrace();
	void MainTrace();
	void InitMainTrace();
	void StopMainTrace();
	
	UFUNCTION(BlueprintCallable)
	void StopGhost_Comp();
	
	UFUNCTION(BlueprintCallable)
	void UpgradeStructure(ABLDStructure* Structure);
	
	//UFUNCTION(BlueprintCallable)
	const FBuildingInfo* GetStructureData(FName StructureName) const;
	const FBuildingInfo* GetUpgradelStructureData(FName StructureName) const;
	bool UpdatePreview_BlueprintFunction(TArray<FBuildingInfo*> StructureInfo);
	
	UFUNCTION(BlueprintNativeEvent)
	void GetRefund();
	
	UFUNCTION(BlueprintCallable)
	void Demolish();
	
	
	
};
