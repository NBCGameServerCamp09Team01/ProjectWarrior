// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAttackIndicator.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AWarriorAttackIndicator::AWarriorAttackIndicator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	IndicatorRoot = CreateDefaultSubobject<USceneComponent>("IndicatorRoot");
	SetRootComponent(IndicatorRoot);

	// 데칼은 로컬 X축으로 투영하므로 아래로 향하게 회전. 회전 후 데칼 Y = 오른쪽, 데칼 Z = 정면
	IndicatorDecal = CreateDefaultSubobject<UDecalComponent>("IndicatorDecal");
	IndicatorDecal->SetupAttachment(IndicatorRoot);
	IndicatorDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
}

void AWarriorAttackIndicator::InitializeIndicator(const FWarriorAttackAreaData& InAreaData, float InFillDuration)
{
	if (IndicatorMaterial)
	{
		IndicatorDecal->SetDecalMaterial(IndicatorMaterial);
	}

	switch (InAreaData.Shape)
	{
	case EWarriorAttackAreaShape::Box:
		IndicatorDecal->DecalSize = FVector(DecalProjectionDepth, InAreaData.Width * 0.5f, InAreaData.Length * 0.5f);
		IndicatorDecal->SetRelativeLocation(FVector(InAreaData.Length * 0.5f, 0.f, 0.f));
		break;

	case EWarriorAttackAreaShape::Circle:
	case EWarriorAttackAreaShape::Cone:
	default:
		IndicatorDecal->DecalSize = FVector(DecalProjectionDepth, InAreaData.Radius, InAreaData.Radius);
		IndicatorDecal->SetRelativeLocation(FVector::ZeroVector);
		break;
	}

	IndicatorDecal->MarkRenderStateDirty();

	IndicatorMID = IndicatorDecal->GetDecalMaterial() ? IndicatorDecal->CreateDynamicMaterialInstance() : nullptr;

	if (IndicatorMID)
	{
		IndicatorMID->SetScalarParameterValue(ShapeParameterName, static_cast<float>(InAreaData.Shape));
		IndicatorMID->SetScalarParameterValue(ConeAngleParameterName, InAreaData.ConeAngle);
	}

	FillDuration = FMath::Max(InFillDuration, 0.f);
	ElapsedTime = 0.f;

	SetFillProgress(FillDuration > 0.f ? 0.f : 1.f);
	SetActorTickEnabled(FillDuration > 0.f);

	if (SafetyLifeSpanAfterFill > 0.f)
	{
		SetLifeSpan(FillDuration + SafetyLifeSpanAfterFill);
	}
}

float AWarriorAttackIndicator::GetFillProgress() const
{
	return FillDuration > 0.f ? FMath::Clamp(ElapsedTime / FillDuration, 0.f, 1.f) : 1.f;
}

void AWarriorAttackIndicator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;

	const float Progress = GetFillProgress();
	SetFillProgress(Progress);

	if (Progress >= 1.f)
	{
		SetActorTickEnabled(false);
	}
}

void AWarriorAttackIndicator::SetFillProgress(float InProgress)
{
	if (IndicatorMID)
	{
		IndicatorMID->SetScalarParameterValue(ProgressParameterName, InProgress);
	}
}
