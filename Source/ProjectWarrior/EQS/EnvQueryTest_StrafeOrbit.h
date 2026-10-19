// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_StrafeOrbit.generated.h"

/**
 * 스트레이프 위치 점수 테스트. 대상(OrbitCenter) 주위를 AI마다 정해진 방향으로 조금씩 돌도록 점수를 매긴다.
 *  - 각도: 현재 내 각도에서 도는 방향(AWarriorAIController::GetStrafeOrbitDirection)으로 MinAngleStep~MaxAngleStep 앞이면 1점,
 *          벗어날수록 AngleFalloff에 걸쳐 0점으로 떨어짐 (뒤로 가는 점은 낮은 점수)
 *  - 반경: PreferredRadius + AI별 보정값(GetStrafeRadiusOffset)에 가까울수록 높은 점수
 * 두 점수를 RadiusWeight 비율로 섞는다.
 */
UCLASS(meta = (DisplayName = "Strafe Orbit"))
class PROJECTWARRIOR_API UEnvQueryTest_StrafeOrbit : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEnvQueryTest_StrafeOrbit(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	// 도는 중심 (보통 BP_EQSContext_TargetActor)
	UPROPERTY(EditDefaultsOnly, Category = "Orbit")
	TSubclassOf<UEnvQueryContext> OrbitCenter;

	// 한 번에 이동하기 좋은 최소/최대 각도. 작을수록 천천히 돈다
	UPROPERTY(EditDefaultsOnly, Category = "Orbit", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float MinAngleStep = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Orbit", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float MaxAngleStep = 50.f;

	// 선호 각도 범위를 벗어났을 때 점수가 0이 되기까지의 각도
	UPROPERTY(EditDefaultsOnly, Category = "Orbit", meta = (ClampMin = "1.0", Units = "deg"))
	float AngleFalloff = 60.f;

	// 기본 선호 반경. AI마다 컨트롤러의 반경 보정값이 더해짐
	UPROPERTY(EditDefaultsOnly, Category = "Radius", meta = (ClampMin = "0.0", Units = "cm"))
	float PreferredRadius = 450.f;

	// 선호 반경에서 이만큼 벗어나면 반경 점수 0
	UPROPERTY(EditDefaultsOnly, Category = "Radius", meta = (ClampMin = "1.0", Units = "cm"))
	float RadiusTolerance = 250.f;

	// 최종 점수에서 반경 점수의 비율 (0 = 각도만, 1 = 반경만)
	UPROPERTY(EditDefaultsOnly, Category = "Radius", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RadiusWeight = 0.3f;

private:
	float ScoreAngle(float DeltaAngle) const;
};
