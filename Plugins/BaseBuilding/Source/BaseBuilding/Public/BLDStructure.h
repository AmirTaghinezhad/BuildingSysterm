// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BLDStructure.generated.h"

enum class EStructureName;
enum class EStructureType : uint8;
class UBoxComponent;
class UBLDComponet;

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
	// Sets default values for this actor's properties
	ABLDStructure();
	void UpdateBuildPreview();

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	UStaticMeshComponent* BuildingMesh;
	
	//UPROPERTY(VisibleAnywhere)
	//UBoxComponent* PlacementBounds;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UBoxComponent* ValidationBox;

	UPROPERTY(BlueprintReadWrite)
	bool bIsbuilt = false;
	
	UPROPERTY(BlueprintReadWrite)
	bool bCanBuild = false;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	bool bShouldSnap = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName StructureName; 

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	FTimerHandle CycleHandle;
	
	bool bBuildStateInitialized = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FSnapSocket> SnapSockets;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	bool bCanFlip = true;
	
private:
	FRotator RotationOffset = FRotator::ZeroRotator;
	
	float CurrentHealth;

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
	bool ValidateBuild(
	bool HasSnapped,
	ABLDStructure* SnappedStructure,
	FName UsedSocketName,
	FName TargetSocketName
);
	
	//A blueprint function to be called after structure is built 
	UFUNCTION(BlueprintNativeEvent)
	void OnStructureBuilt();
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Building")
	bool ValidateBuild_BlueprintSide(
	ABLDStructure* SnappedStructure,
	FName UsedSocketName,
	FName TargetSocketName
);
	
	float GetHealth();
	FName GetStructureName();
	void Add_DepleatHealth(float Health);
	void UpgradeStructure();
};
