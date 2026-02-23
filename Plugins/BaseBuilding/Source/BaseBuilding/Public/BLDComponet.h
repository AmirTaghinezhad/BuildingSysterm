// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BLDComponet.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FBuildingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FName BuildingName;
	
	UPROPERTY(BlueprintReadWrite)
	AActor* BuildingClass;
	
	UPROPERTY(BlueprintReadWrite)
	AActor* MaterialClass;

	UPROPERTY(BlueprintReadWrite)
	float RequiredAmount;

	UPROPERTY(BlueprintReadWrite)
	FName MaterialName;
	
};



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BASEBUILDING_API UBLDComponet : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBLDComponet();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void ToggleBuildMode();
	void BuildCycle();
	void SpawnBuildGhost();
	void UpdateBuildPreview();

	bool bDoOnce = true;

	FTimerHandle CycleHandle;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	class UCameraComponent* CameraRef;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	UStaticMeshComponent* BuildGhost;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FTransform BuildTransform;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	bool IsBuildMoodOn;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	ACharacter* PlayerRefrence;
	
};
