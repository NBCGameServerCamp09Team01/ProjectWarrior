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

protected:
	virtual void BeginPlay() override;

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
};
