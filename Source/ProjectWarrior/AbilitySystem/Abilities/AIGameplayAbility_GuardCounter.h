// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_GuardCounter.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 가드 자세를 유지하다가 플레이어 근접 공격을 막으면 잠시 후 범위 반격하는 AI 어빌리티 (엘리트용).
 *  1. 가드 몽타주 재생 + AI.Status.Guarding. 정면에서 온 플레이어 근접 공격은 막힘 (UPlayerCombatComponent)
 *  2. GuardDuration 동안 막지 못하면 가드를 풀고 종료 -> BT가 원래 패턴으로 돌아감
 *  3. 막으면(AI.Event.GuardHit) 막기 반응 몽타주, CounterDelay 뒤 반격 몽타주 + 위험 범위 표시
 *     -> 몽타주의 MontageImpactEventTag 노티파이 시점에 범위 판정 -> 종료
 * 반격 중에는 슈퍼아머(AI.Status.SuperArmor). 뒤나 옆에서 맞으면 피격 경직이 가드를 취소한다 (AI.Ability.Block 하위 태그).
 * BT에서는 BTTask_ActivateAbilityAndWait로 실행해 끝날 때까지 기다린다.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_GuardCounter : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_GuardCounter();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 공격을 막지 못했을 때 가드를 유지하는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Guard", meta = (ClampMin = "0.1", Units = "s"))
	float GuardDuration = 3.f;

	// 처음 막은 순간부터 반격 시작까지의 시간
	UPROPERTY(EditDefaultsOnly, Category = "Guard", meta = (ClampMin = "0.0", Units = "s"))
	float CounterDelay = 1.f;

	// 가드 유지 몽타주. 반복 구간이 없으면 끝날 때 다시 재생
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Montage")
	TObjectPtr<UAnimMontage> GuardMontage;

	// 막았을 때 무작위로 재생. 재생 중에 또 막으면 새로 재생하지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Montage")
	TArray<TObjectPtr<UAnimMontage>> GuardHitMontages;

	// 가드 몽타주 안의 가드 해제 구간 (예: BlockEnd). 있으면 가드를 풀 때 이 구간으로 넘어가 끝까지 재생
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Montage")
	FName GuardEndSection;

	// 가드 유지 구간 (예: Block). 이 재생에서만 반복하고, 막기 반응 후에도 여기서 재개 (비워 두면 처음부터 = 가드를 다시 올림)
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Montage")
	FName GuardResumeSection;

	// 막지 못하고 가드를 풀 때 재생. GuardEndSection이 있으면 사용하지 않음 (둘 다 없으면 가드 몽타주를 블렌드 아웃)
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Montage")
	TObjectPtr<UAnimMontage> GuardEndMontage;

	// 반격 몽타주. MontageImpactEventTag(기본 AI.Event.Boss.AreaImpact) 노티파이 시점에 DefaultAreaData 범위로 판정
	UPROPERTY(EditDefaultsOnly, Category = "Counter")
	TObjectPtr<UAnimMontage> CounterMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Counter")
	TSubclassOf<UGameplayEffect> CounterDamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Counter")
	FScalableFloat CounterDamage;

	// 반격 동작 중 피격 경직을 막음
	UPROPERTY(EditDefaultsOnly, Category = "Counter")
	bool bSuperArmorDuringCounter = true;

private:
	enum class EGuardPhase : uint8
	{
		None,
		Guarding,
		Ending,
		Countering
	};

	void PlayGuardMontage(FName StartSection = NAME_None);
	void SetOwnerTag(const FGameplayTag& Tag, bool bEnable, bool& bState);

	UFUNCTION()
	void OnGuardMontageEnded();

	UFUNCTION()
	void OnGuardHit(FGameplayEventData Payload);

	UFUNCTION()
	void OnGuardHitMontageEnded();

	UFUNCTION()
	void OnGuardTimeout();

	UFUNCTION()
	void StartCounter();

	UFUNCTION()
	void OnCounterImpact(FGameplayEventData Payload);

	UFUNCTION()
	void OnFinalMontageEnded();

	UPROPERTY()
	TObjectPtr<UAbilityTask> GuardTimeoutTask;

	EGuardPhase Phase = EGuardPhase::None;
	bool bPlayingGuardHit = false;
	bool bCounterScheduled = false;
	bool bGuardingTag = false;
	bool bSuperArmorTag = false;
};
