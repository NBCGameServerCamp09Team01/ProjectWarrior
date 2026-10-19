// Fill out your copyright notice in the Description page of Project Settings.


#include "EnvQueryTest_StrafeOrbit.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "ProjectWarrior/Controllers/WarriorAIController.h"
#include "GameFramework/Pawn.h"

UEnvQueryTest_StrafeOrbit::UEnvQueryTest_StrafeOrbit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	TestPurpose = EEnvTestPurpose::Score;
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_StrafeOrbit::RunTest(FEnvQueryInstance& QueryInstance) const
{
	const AActor* QuerierActor = Cast<AActor>(QueryInstance.Owner.Get());

	TArray<FVector> CenterLocations;
	if (!QuerierActor || !OrbitCenter || !QueryInstance.PrepareContext(OrbitCenter, CenterLocations) || CenterLocations.IsEmpty())
	{
		return;
	}

	int32 OrbitDirection = 1;
	float RadiusOffset = 0.f;

	if (const APawn* QuerierPawn = Cast<APawn>(QuerierActor))
	{
		if (const AWarriorAIController* AIController = Cast<AWarriorAIController>(QuerierPawn->GetController()))
		{
			OrbitDirection = AIController->GetStrafeOrbitDirection();
			RadiusOffset = AIController->GetStrafeRadiusOffset();
		}
	}

	const FVector Center = CenterLocations[0];
	const FVector QuerierOffset = QuerierActor->GetActorLocation() - Center;
	const float QuerierAngle = FMath::RadiansToDegrees(FMath::Atan2(QuerierOffset.Y, QuerierOffset.X));
	const float TargetRadius = FMath::Max(PreferredRadius + RadiusOffset, 0.f);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemOffset = GetItemLocation(QueryInstance, It.GetIndex()) - Center;
		const float ItemAngle = FMath::RadiansToDegrees(FMath::Atan2(ItemOffset.Y, ItemOffset.X));

		// 도는 방향으로 얼마나 앞에 있는지 (음수면 반대 방향)
		const float DeltaAngle = FMath::FindDeltaAngleDegrees(QuerierAngle, ItemAngle) * OrbitDirection;
		const float AngleScore = ScoreAngle(DeltaAngle);

		const float RadiusScore = FMath::Clamp(1.f - FMath::Abs(ItemOffset.Size2D() - TargetRadius) / RadiusTolerance, 0.f, 1.f);

		const float Score = FMath::Lerp(AngleScore, RadiusScore, RadiusWeight);
		It.SetScore(TestPurpose, FilterType, Score, 0.f, 1.f);
	}
}

float UEnvQueryTest_StrafeOrbit::ScoreAngle(float DeltaAngle) const
{
	const float MinStep = FMath::Min(MinAngleStep, MaxAngleStep);
	const float MaxStep = FMath::Max(MinAngleStep, MaxAngleStep);

	if (DeltaAngle < MinStep)
	{
		return FMath::Clamp(1.f - (MinStep - DeltaAngle) / AngleFalloff, 0.f, 1.f);
	}

	if (DeltaAngle > MaxStep)
	{
		return FMath::Clamp(1.f - (DeltaAngle - MaxStep) / AngleFalloff, 0.f, 1.f);
	}

	return 1.f;
}

FText UEnvQueryTest_StrafeOrbit::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("Strafe Orbit around %s"), *UEnvQueryTypes::DescribeContext(OrbitCenter).ToString()));
}

FText UEnvQueryTest_StrafeOrbit::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("step %.0f~%.0f deg, radius %.0f +-%.0f (weight %.2f)"),
		MinAngleStep, MaxAngleStep, PreferredRadius, RadiusTolerance, RadiusWeight));
}
