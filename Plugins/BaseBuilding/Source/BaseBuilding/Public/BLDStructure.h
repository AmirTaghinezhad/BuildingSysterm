// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BLDStructure.generated.h"

UCLASS()
class BASEBUILDING_API ABLDStructure : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABLDStructure();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
