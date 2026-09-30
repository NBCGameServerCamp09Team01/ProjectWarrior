// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SelectBossPattern.generated.h"

/**
 * UBossPatternComponent로 다음 패턴을 골라 태그 이름을 Name 키에 기록한다.
 * 후보가 없으면 키를 비우고 Failed (Selector의 다음 분기 = 접근/대기 등으로 넘어감)
 */
UCLASS()
class PROJECTWARRIOR_API UBTTask_SelectBossPattern : public UBTTaskNode
{
	GENERATED_BODY()

	UBTTask_SelectBossPattern();

	//~ Begin UBTNode Interface
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Boss Pattern")
	FBlackboardKeySelector InTargetActorKey;

	// 선택된 패턴의 어빌리티 태그 이름을 기록할 Name 키
	UPROPERTY(EditAnywhere, Category = "Boss Pattern")
	FBlackboardKeySelector OutPatternTagKey;
};
