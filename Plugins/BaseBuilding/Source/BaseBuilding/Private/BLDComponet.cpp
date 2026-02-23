// Fill out your copyright notice in the Description page of Project Settings.


#include "BLDComponet.h"

#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"

// Sets default values for this component's properties
UBLDComponet::UBLDComponet()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UBLDComponet::BeginPlay()
{
	Super::BeginPlay();

	PlayerRefrence = Cast<ACharacter>(GetOwner());
	if (!PlayerRefrence) {UE_LOG(LogTemp, Error, TEXT("Player Reference is NULL!"));}
	
}

// Called every frame
void UBLDComponet::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}



void UBLDComponet::ToggleBuildMode()
{
	if (!IsBuildMoodOn)
	{
		PlayerRefrence->GetWorldTimerManager().SetTimer(CycleHandle, this, &UBLDComponet::UpdateBuildPreview, 0.01f, true);
		IsBuildMoodOn = true;
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(CycleHandle);
		if (BuildGhost) {BuildGhost->DestroyComponent();}
		IsBuildMoodOn = false;
		bDoOnce = true;
	}
}

void UBLDComponet::BuildCycle()
{
	FVector StartLocation = CameraRef->GetComponentLocation() + CameraRef->GetForwardVector() * 350.0f;
	FVector EndLocation = CameraRef->GetComponentLocation() + CameraRef->GetForwardVector() * 1000.0f;
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(GetOwner());

	bool Hit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, CollisionParams);

	if (Hit)
	{
		BuildTransform.SetLocation(HitResult.ImpactNormal);
	}
	else
	{
		BuildTransform.SetLocation(HitResult.TraceEnd);
	}
}

void UBLDComponet::SpawnBuildGhost()
{
}

void UBLDComponet::UpdateBuildPreview()
{
	if (bDoOnce)
	{
		SpawnBuildGhost();
		bDoOnce = false;
	}
	BuildCycle();
	if(BuildGhost)
	{
		BuildGhost->SetWorldLocation(BuildTransform.GetLocation());
		BuildGhost->SetWorldRotation(FRotator::ZeroRotator);
	}
}
