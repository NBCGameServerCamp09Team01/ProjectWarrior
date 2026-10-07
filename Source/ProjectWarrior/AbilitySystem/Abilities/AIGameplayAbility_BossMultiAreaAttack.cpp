// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossMultiAreaAttack.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"

UAIGameplayAbility_BossMultiAreaAttack::UAIGameplayAbility_BossMultiAreaAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UAIGameplayAbility_BossMultiAreaAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 부모가 토큰 부족 등으로 이미 끝냈으면 진행하지 않음
	if (!IsActive())
	{
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossMultiAreaAttack] %s has no attack montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CurrentHitIndex = 0;
	FindMontageEventTimes(AttackMontage, MontageImpactEventTag, ImpactTimes);

	if (ImpactTimes.IsEmpty())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossMultiAreaAttack] %s: notify with %s not found in %s."),
			*GetName(), *MontageImpactEventTag.ToString(), *AttackMontage->GetName());
	}

	// 재생 전에 표시해야 루트 모션을 몽타주 처음부터 계산함
	TelegraphHit(0, bFaceTargetOnStart);

	UAbilityTask_WaitGameplayEvent* ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, MontageImpactEventTag, nullptr, false, true);
	ImpactTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactEvent);
	ImpactTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_BossMultiAreaAttack::HandleImpactEvent(FGameplayEventData Payload)
{
	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	ApplyAreaDamage(DamageSpecHandle, true);

	// 다음 판정이 있으면 바로 그 범위를 표시
	++CurrentHitIndex;
	TelegraphHit(CurrentHitIndex, bFaceTargetBeforeEachHit);
}

void UAIGameplayAbility_BossMultiAreaAttack::HandleMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_BossMultiAreaAttack::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool UAIGameplayAbility_BossMultiAreaAttack::TelegraphHit(int32 HitIndex, bool bFaceTarget)
{
	if (!ImpactTimes.IsValidIndex(HitIndex))
	{
		return false;
	}

	return BeginMontageAreaTelegraphAtTime(AttackMontage, ImpactTimes[HitIndex], GetHitArea(HitIndex), nullptr, bFaceTarget);
}

const FWarriorAttackAreaData& UAIGameplayAbility_BossMultiAreaAttack::GetHitArea(int32 HitIndex) const
{
	return HitAreas.IsValidIndex(HitIndex) ? HitAreas[HitIndex] : DefaultAreaData;
}
