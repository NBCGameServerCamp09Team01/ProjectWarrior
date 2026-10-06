// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "BTTask_ActivateAbilityAndWait.generated.h"

class UAbilitySystemComponent;
struct FAbilityEndedData;

/**
 * AbilityTag와 일치하는 어빌리티를 발동하고 끝날 때까지 기다리는 태스크.
 * (BT_ActivateAbilityByTag는 발동 즉시 끝나서 가드처럼 오래 걸리는 어빌리티 도중에 BT가 다음 행동으로 넘어감)
 * 발동하지 못하면 Failed, 정상 종료면 Succeeded, 취소되면 Failed. 태스크가 중단되면 어빌리티도 취소(bCancelAbilityOnAbort)
 */
UCLASS()
class PROJECTWARRIOR_API UBTTask_ActivateAbilityAndWait : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateAbilityAndWait();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (Categories = "AI.Ability"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bCancelAbilityOnAbort = true;

private:
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void StopListening();

	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FDelegateHandle AbilityEndedHandle;
	// TryActivateAbility 호출 중 (그 안에서 끝나면 FinishLatentTask 대신 ExecuteTask가 결과 반환)
	bool bActivating = false;
};
