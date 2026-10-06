// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WarriorAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	AWarriorAIController(const FObjectInitializer& ObjectInitializer);

	//~ Begin IGenericTeamAgentInterface Interface.
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~ End IGenericTeamAgentInterface Interface

	//~ Begin AAIController Interface.
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;
	//~ End AAIController Interface

	// 감지 없이 처음부터 대상을 알고 시작 (웨이브 스폰 등). 블랙보드가 아직 없으면 BT 실행 시점에 적용
	UFUNCTION(BlueprintCallable, Category = "Warrior|AI")
	void SetInitialTarget(AActor* InTarget);

	// 스트레이프로 대상 주위를 도는 방향. 1 = Yaw가 커지는 쪽(위에서 보면 시계 방향), -1 = 반대
	UFUNCTION(BlueprintPure, Category = "Warrior|AI|Strafe")
	int32 GetStrafeOrbitDirection() const { return StrafeOrbitDirection; }

	// 이 AI의 선호 스트레이프 반경 보정값. AI마다 달라서 한 원 위에 줄지어 서지 않음
	UFUNCTION(BlueprintPure, Category = "Warrior|AI|Strafe")
	float GetStrafeRadiusOffset() const { return StrafeRadiusOffset; }

	// 도는 방향을 뒤집음 (막혔을 때 BT 등에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Warrior|AI|Strafe")
	void FlipStrafeOrbitDirection();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAIPerceptionComponent* AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAISenseConfig_Sight* AISenseConfig_Sight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAISenseConfig_Hearing* AISenseConfig_Hearing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAISenseConfig_Damage* AISenseConfig_Damage;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config")
	bool bEnableDetourCrowdAvoidance = true;

	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config", meta = (EditCondition = "bEnableDetourCrowdAvoidance", UIMin = "1", UIMax = "4"))
	int32 DetourCrowdAvoidanceQuality = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config", meta = (EditCondition = "bEnableDetourCrowdAvoidance"))
	float CollisionQueryRange = 600.f;

	// 블랙보드 TargetActor가 비어 있으면 InTarget을 넣음. 넣었으면 true
	bool TrySetTargetActor(AActor* InTarget);

	// BT 실행 전에 SetInitialTarget으로 받은 대상
	TWeakObjectPtr<AActor> PendingInitialTarget;

	// 선호 스트레이프 반경 보정 범위 (±). BeginPlay에서 이 범위 안의 값을 무작위로 정함
	UPROPERTY(EditDefaultsOnly, Category = "Strafe", meta = (ClampMin = "0.0", Units = "cm"))
	float StrafeRadiusVariance = 100.f;

	// 도는 방향을 저절로 뒤집는 간격 (초). 최솟값~최댓값 사이에서 무작위. 최댓값이 0이면 저절로 뒤집지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Strafe", meta = (ClampMin = "0.0", Units = "s"))
	FVector2D StrafeOrbitFlipInterval = FVector2D(8.f, 16.f);

	void ScheduleStrafeOrbitFlip();
	void DrawStrafeDebug() const;

	int32 StrafeOrbitDirection = 1;
	float StrafeRadiusOffset = 0.f;
	FTimerHandle StrafeOrbitFlipTimerHandle;
};
