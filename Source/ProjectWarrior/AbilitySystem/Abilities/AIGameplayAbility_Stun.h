// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "AIGameplayAbility_Stun.generated.h"

class UAnimMontage;

/**
 * 인간형 적이 같이 쓰는 기절(균형 붕괴). 균형이 0이 되면 받는 Shared.Event.Stagger로 켜진다(UWarriorAttributeSet).
 * - 기절 동안: 행동 트리를 멈추고 이동을 세우며, 공격·막기·회피·특수·무기 장착·피격 반응을 취소하고 막는다. 처형은 막지 않는다
 * - 상태 태그 Shared.Status.Stunned를 붙인다
 * - 몽타주가 끝나면 균형을 가득 채우고 행동 트리를 다시 돌린다(한 번 무너진 뒤 계속 0으로 남지 않게)
 * 기절 시간 = 몽타주 길이 ÷ StunPlayRate. 비인간형은 각자 스태거 GA(GA_Rampage_Stagger 등)를 쓴다.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_Stun : public UAIGameplayAbility
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_Stun();

protected:
	//~ Begin UGameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	//기절 몽타주(AM_AIStun_Humanoid). 여러 개면 무작위
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stun")
	TArray<TObjectPtr<UAnimMontage>> StunMontages;

	//몽타주 재생 속도. 낮추면 기절이 길어진다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stun", meta = (ClampMin = "0.1"))
	float StunPlayRate = 1.f;

	//끝날 때 균형을 가득 채운다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stun")
	bool bRestoreBalanceOnEnd = true;

private:
	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	void SetBehaviorPaused(bool bInPaused) const;
};
