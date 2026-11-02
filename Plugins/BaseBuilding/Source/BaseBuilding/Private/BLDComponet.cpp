// Fill out your copyright notice in the Description page of Project Settings.


#include "BLDComponet.h"

#include "BLDStructure.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"


UBLDComponet::UBLDComponet()
{
	
	PrimaryComponentTick.bCanEverTick = true;

}



void UBLDComponet::BeginPlay()
{
	Super::BeginPlay();

	PlayerRefrence = Cast<ACharacter>(GetOwner());
	if (!PlayerRefrence) {UE_LOG(LogTemp, Error, TEXT("Player Reference is NULL!"));}
	
}

void UBLDComponet::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UBLDComponet::AttemptRotation(float YawOffset)
{
	if (BuildGhost)
	{
		BuildGhost->RotationOfset(YawOffset);

	}
}

void UBLDComponet::ToggleBuildMode(TSubclassOf<ABLDStructure> Structure)
{
	if (!IsBuildMoodOn)
	{
		
			//SpawnBuildGhost(Structure);
			bDoOnce = false;
		
		IsBuildMoodOn = true;
		InitMainTrace();
	}
	else
	{
		if (!BuildGhost)
		{
			IsBuildMoodOn = false;
			StopMainTrace();
			return;
		}
		if (BuildGhost->bIsbuilt)
		{
			BuildGhost = nullptr;
		}
		else
		{
			BuildGhost->StopGhost(); //Make a unique timer stop
			BuildGhost->Destroy();
			BuildGhost = nullptr;
		}
		IsBuildMoodOn = false;
		//bDoOnce = true;
	}
}


void UBLDComponet::SpawnBuildGhost(TSubclassOf<ABLDStructure> Structure)
{
	if (!IsBuildMoodOn) return;
	if (BuildGhost)
	{
		BuildGhost->StopGhost();
		BuildGhost->Destroy();
		BuildGhost = nullptr;
	}
	StopMainTrace();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerRefrence;

	BuildGhost = GetWorld()->SpawnActor<ABLDStructure>(Structure, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	BuildGhost->BLDComponent = this;
	BuildGhost->InitGhost();
}

void UBLDComponet::MainTrace()
{
	//These should neve occur here
	//if (!IsBuildMoodOn) return;
	//if (BuildGhost) return;
	FVector StartLocation = CameraRef->GetComponentLocation() +  CameraRef->GetForwardVector() * 350.0f;
	FVector EndLocation = CameraRef->GetComponentLocation() +  CameraRef->GetForwardVector() * 600.0f;
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(GetOwner());
	bool Hit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
	DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red,false, 5.0f);
	if (Hit)
	{
		if (TargetStructure == Cast<ABLDStructure>(HitResult.GetActor())) return;
		TargetStructure = nullptr;
		TargetStructure = Cast<ABLDStructure>(HitResult.GetActor());
		if (TargetStructure) DrawDebugBox(GetWorld(), TargetStructure->GetActorLocation(), TargetStructure->ValidationBox->GetScaledBoxExtent(),TargetStructure->ValidationBox->GetComponentRotation().Quaternion(), FColor::Red, false, 1.0f);
		OnTargetStructureUpdate.Broadcast(TargetStructure);
	}
	else
	{
		if (TargetStructure != nullptr)
		{
			TargetStructure = nullptr;
			OnTargetStructureUpdate.Broadcast(TargetStructure);
			
			
		}
	}
}

FVector UBLDComponet::GhostTrace()
{
	FVector StartLocation = CameraRef->GetComponentLocation() +  CameraRef->GetForwardVector() * 350.0f;
	FVector EndLocation = CameraRef->GetComponentLocation() +  CameraRef->GetForwardVector() * 750.0f;
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(GetOwner());
	bool Hit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
	DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red,false, 5.0f);
	
	if (Hit)
	{
		FVector HitLocation = HitResult.ImpactPoint;
		return HitLocation;
		//UE_LOG(LogTemp, Log, TEXT("BLDStructure: Hit is true, Hit locations is %s"), *HitResult.ImpactNormal.ToString());
		
	}
	else
	{
		FVector TraceEnd = (HitResult.TraceEnd);
		return TraceEnd;
		//UE_LOG(LogTemp, Log, TEXT("BLDStructure: Hit is false"));
	}
	
}

void UBLDComponet::BuildStructure()
{
	if (!IsBuildMoodOn || !BuildGhost) return;	
	if (!BuildGhost->bCanBuild) return;
	/*
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerRefrence;
	
	ABLDStructure* SpawnedStructure = GetWorld()->SpawnActor<ABLDStructure>(BuildGhost->GetClass(), BuildGhost->GetActorLocation(), BuildGhost->GetActorRotation(), SpawnParams);
	SpawnedStructure->BLDComponent = this; 
	*/
	BuildGhost->StopGhost();
	BuildGhost->bIsbuilt = true;
	BuildGhost->OnStructureBuilt();
	TSubclassOf<ABLDStructure> BLDCalss = BuildGhost->GetClass();
	BuildGhost = nullptr;
	SpawnBuildGhost(BLDCalss);
}


void UBLDComponet::InitMainTrace()
{
	GetWorld()->GetTimerManager().SetTimer(MainTraceCycleHandle, this, &UBLDComponet::MainTrace, 0.1f, true);
}

void UBLDComponet::StopMainTrace()
{
	//if (!MainTraceCycleHandle.IsValid()) return;
	GetWorld()->GetTimerManager().ClearTimer(MainTraceCycleHandle);
	
}

void UBLDComponet::UpgradeStructure(ABLDStructure* Structure)
{
	if (!IsBuildMoodOn || !Structure) return;
	if (!Structure->bIsbuilt) return;
	Structure->UpgradeStructure();
}

const FBuildingInfo* UBLDComponet::GetStructureData(FName StructureName) const
{

	if (!StructureDataTable)
		return nullptr;

	return StructureDataTable->FindRow<FBuildingInfo>(
		StructureName,
		TEXT("GetStructureData"));
}

const FBuildingInfo* UBLDComponet::GetUpgradelStructureData(FName StructureName) const
{
	const FBuildingInfo* NexLevelStructure = GetStructureData(StructureName);
	if (!NexLevelStructure) return nullptr;
	return GetStructureData(NexLevelStructure->NextLevelStructureRowName);
}

bool UBLDComponet::UpdatePreview_BlueprintFunction(TArray<FBuildingInfo*> StructureInfo)
{
	return false;
}

void UBLDComponet::Demolish()
{
	if (!TargetStructure) return;
	GetRefund();
	TargetStructure->Destroy();
	TargetStructure = nullptr;
	OnTargetStructureUpdate.Broadcast(TargetStructure);
}

void UBLDComponet::GetRefund_Implementation()
{
	
}

void UBLDComponet::StopGhost_Comp()
{
	if (!BuildGhost) return;
	BuildGhost->StopGhost();
}