// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayAbilitySpecHandle.h"
#include "BTTask_ActivateBossPattern.generated.h"

class UAbilitySystemComponent;

struct FActivateBossPatternTaskMemory
{
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	FGameplayAbilitySpecHandle ActivatedSpecHandle;

	void Reset()
	{
		AbilitySystemComponent.Reset();
		ActivatedSpecHandle = FGameplayAbilitySpecHandle();
	}
};

/**
 * Name 키에 기록된 패턴 태그의 어빌리티를 발동하고, 성공하면 UBossPatternComponent에 알린다(패턴 쿨다운 시작).
 * bWaitForAbilityEnd면 어빌리티가 끝날 때까지 InProgress.
 */
UCLASS()
class PROJECTWARRIOR_API UBTTask_ActivateBossPattern : public UBTTaskNode
{
	GENERATED_BODY()

	UBTTask_ActivateBossPattern();

	//~ Begin UBTNode Interface
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	// UBTTask_SelectBossPattern이 기록한 Name 키
	UPROPERTY(EditAnywhere, Category = "Boss Pattern")
	FBlackboardKeySelector InPatternTagKey;

	UPROPERTY(EditAnywhere, Category = "Boss Pattern")
	bool bWaitForAbilityEnd = true;

	// 태스크가 중단(BT 분기 전환 등)되면 진행 중인 패턴 어빌리티를 취소
	UPROPERTY(EditAnywhere, Category = "Boss Pattern", meta = (EditCondition = "bWaitForAbilityEnd"))
	bool bCancelAbilityOnAbort = false;

	// 발동 후 키를 비움 (같은 패턴이 실수로 다시 발동되는 것 방지)
	UPROPERTY(EditAnywhere, Category = "Boss Pattern")
	bool bClearPatternKeyOnActivate = true;
};
