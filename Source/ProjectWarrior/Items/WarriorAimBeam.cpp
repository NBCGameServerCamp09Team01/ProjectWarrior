// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAimBeam.h"
#include "NiagaraComponent.h"

AWarriorAimBeam::AWarriorAimBeam()
{
	PrimaryActorTick.bCanEverTick = true;
	// 소켓을 따라 움직인 뒤(애니메이션 갱신 후) 끝점을 맞춤
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	BeamComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BeamComponent"));
	SetRootComponent(BeamComponent);
}

void AWarriorAimBeam::InitializeBeam(USceneComponent* InSourceComponent, FName InSourceSocket, AActor* InTarget)
{
	Target = InTarget;
	bLocked = false;
	bEndFrozen = false;

	if (InSourceComponent)
	{
		AttachToComponent(InSourceComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, InSourceSocket);
	}

	BeamComponent->SetVariableLinearColor(BeamColorParameterName, TrackingColor);
	UpdateBeamEnd();
}

void AWarriorAimBeam::LockBeam(bool bFreezeEnd)
{
	if (bLocked)
	{
		return;
	}

	bLocked = true;
	bEndFrozen = bFreezeEnd;

	if (bEndFrozen)
	{
		LockedEnd = Target.IsValid() ? Target->GetActorLocation() + TargetOffset : GetActorLocation() + GetActorForwardVector() * FallbackLength;
	}

	BeamComponent->SetVariableLinearColor(BeamColorParameterName, LockedColor);
	UpdateBeamEnd();

	BP_OnBeamLocked();
}

void AWarriorAimBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateBeamEnd();
}

void AWarriorAimBeam::UpdateBeamEnd()
{
	if (bEndFrozen)
	{
		BeamComponent->SetVariableVec3(BeamEndParameterName, LockedEnd);
		return;
	}

	const FVector End = Target.IsValid()
		? Target->GetActorLocation() + TargetOffset
		: GetActorLocation() + GetActorForwardVector() * FallbackLength;

	BeamComponent->SetVariableVec3(BeamEndParameterName, End);
}
