// Fill out your copyright notice in the Description page of Project Settings.


#include "BLDStructure.h"


// Sets default values
ABLDStructure::ABLDStructure()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABLDStructure::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABLDStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

