// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "GameplayTagContainer.h"
#include "BTDecorator_AttackToken.generated.h"

class UAttackTokenComponent;

/**
 * 대상의 공격 토큰을 받을 수 있을 때만 하위 분기(접근 + 공격)를 실행하는 데코레이터.
 * 분기가 시작되면 토큰을 받아 두고(예약), 분기가 끝나거나 중단되면 반납한다. 분기 안의 공격 어빌리티는 이 토큰을 그대로 사용한다.
 * Observer Aborts를 Both로 두면 토큰이 생겼을 때 스트레이프를 끊고 들어오고, 토큰을 잃으면 접근을 끊고 스트레이프로 돌아간다.
 */
UCLASS()
class PROJECTWARRIOR_API UBTDecorator_AttackToken : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_AttackToken();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData) override;
	virtual void OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// 토큰을 가진 공격 대상
	UPROPERTY(EditAnywhere, Category = "AttackToken")
	FBlackboardKeySelector TargetActorKey;

	// 공격 어빌리티의 AttackTokenPool과 같게 맞춤 (근접 AI는 Melee, 원거리 AI는 Range)
	UPROPERTY(EditAnywhere, Category = "AttackToken", meta = (Categories = "AI.AttackToken"))
	FGameplayTag TokenPool;

	UPROPERTY(EditAnywhere, Category = "AttackToken", meta = (ClampMin = "1"))
	int32 TokenCost = 1;

	// 토큰 상태를 다시 확인하는 주기. 바뀌었으면 Observer Aborts 설정에 따라 분기를 전환
	UPROPERTY(EditAnywhere, Category = "AttackToken", meta = (ClampMin = "0.05", Units = "s"))
	float CheckInterval = 0.2f;

private:
	struct FAttackTokenDecoratorMemory
	{
		// 이 데코레이터가 받은 토큰. 반납할 곳
		TWeakObjectPtr<UAttackTokenComponent> AcquiredFrom;
		float TimeUntilCheck = 0.f;
		// 마지막으로 확인한 조건 결과. 바뀐 순간에만 분기 전환을 요청
		bool bLastCondition = false;
	};

	UAttackTokenComponent* FindTokenComponent(const UBehaviorTreeComponent& OwnerComp) const;
	void ReleaseToken(UBehaviorTreeComponent& OwnerComp, FAttackTokenDecoratorMemory& Memory) const;
};
