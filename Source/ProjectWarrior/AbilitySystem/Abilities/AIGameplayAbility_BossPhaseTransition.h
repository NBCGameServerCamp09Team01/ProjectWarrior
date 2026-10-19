// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "AIGameplayAbility_BossPhaseTransition.generated.h"

class UGameplayEffect;

/**
 * 보스 페이즈 전환 연출. AWarriorBossCharacter::SetPhase가 보내는 AI.Event.Boss.PhaseChanged로 발동한다.
 * 하던 행동·경직을 끊고, 연출 동안 무적·슈퍼아머로 BT를 멈춘 채 포효 몽타주를 재생한다.
 * 몽타주의 판정 노티파이(MontageImpactEventTag) 시점에 DefaultAreaData 범위로 충격파 (피해 없이 HitReactEventTag만 보냄, 기본 KnockBack)
 * 위험 범위는 AI.Event.Boss.Telegraph 노티파이부터 표시 (없으면 몽타주 시작부터)
 * 보스 StartUpData에 부여해야 함 (패턴 목록에는 넣지 않음)
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossPhaseTransition : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BossPhaseTransition();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 포효 몽타주. 비어 있으면 페이즈 효과만 적용하고 바로 끝남
	UPROPERTY(EditDefaultsOnly, Category = "PhaseTransition")
	TObjectPtr<UAnimMontage> TransitionMontage;

	// 진입한 페이즈 -> 자신에게 적용할 효과 (공격력 증가 등, Infinite 권장). 어빌리티가 끝나도 유지됨
	UPROPERTY(EditDefaultsOnly, Category = "PhaseTransition")
	TMap<int32, TSubclassOf<UGameplayEffect>> PhaseEffects;

	// 판정 시점에 충격파를 낼지. 그로기(스태거) 중에 전환되면 이 값과 무관하게 내지 않음
	UPROPERTY(EditDefaultsOnly, Category = "PhaseTransition")
	bool bApplyShockwave = true;

	// 연출 동안 BT를 멈춤 (이동·패턴 선택 중단). 끝나면 다시 시작
	UPROPERTY(EditDefaultsOnly, Category = "PhaseTransition")
	bool bPauseBehaviorTree = true;

private:
	UFUNCTION()
	void HandleTelegraphEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleShockwaveEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	void ApplyPhaseEffect(int32 NewPhase);

	void SetBehaviorTreePaused(bool bPaused);

	bool bPausedBehaviorTree = false;
};
