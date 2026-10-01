// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_UpdateBossTargetInfo.generated.h"

/**
 * 대상과의 거리·각도를 Float 키에 기록한다. 패턴 선택과 같은 기준(UBossPatternComponent::GetTargetDistanceAndAngle)을 사용하므로
 * "거리 > N 이면 추격" 같은 데코레이터 조건이 패턴 데이터의 사거리와 어긋나지 않는다.
 */
UCLASS()
class PROJECTWARRIOR_API UBTS_UpdateBossTargetInfo : public UBTService
{
	GENERATED_BODY()

	UBTS_UpdateBossTargetInfo();

	//~ Begin UBTNode Interface
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual FString GetStaticDescription() const override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	//~ End UBTNode Interface

	UPROPERTY(EditAnywhere, Category = "Target")
	FBlackboardKeySelector InTargetActorKey;

	// 비워 두면 기록하지 않음
	UPROPERTY(EditAnywhere, Category = "Target")
	FBlackboardKeySelector OutDistanceKey;

	// 비워 두면 기록하지 않음
	UPROPERTY(EditAnywhere, Category = "Target")
	FBlackboardKeySelector OutAngleKey;
};
