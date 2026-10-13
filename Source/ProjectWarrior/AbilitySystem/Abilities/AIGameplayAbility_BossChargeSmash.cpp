// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossChargeSmash.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

UAIGameplayAbility_BossChargeSmash::UAIGameplayAbility_BossChargeSmash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FinishAreaData.Shape = EWarriorAttackAreaShape::Circle;
	FinishAreaData.Radius = 300.f;
}

void UAIGameplayAbility_BossChargeSmash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	BeginCharge(nullptr, ChargeDuration);
	PlayLoopMontage(ChargeMontage);

	UAbilityTask_WaitDelay* ChargeTask = UAbilityTask_WaitDelay::WaitDelay(this, ChargeDuration);
	ChargeTask->OnFinish.AddDynamic(this, &ThisClass::HandleChargeFinished);
	ChargeTask->ReadyForActivation();
}

void UAIGameplayAbility_BossChargeSmash::HandleChargeFinished()
{
	const FGameplayEffectSpecHandle DashSpecHandle = DashDamageEffectClass
		? MakeAIDamageEffectSpecHandle(DashDamageEffectClass, DashDamageScalableFloat)
		: FGameplayEffectSpecHandle();

	if (!StartDash(DashSpecHandle, DashDuration))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayLoopMontage(DashMontage);
}

void UAIGameplayAbility_BossChargeSmash::OnChargeDashFinished_Implementation()
{
	if (!FinishMontage)
	{
		StopLoopMontages();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 판정 시점의 보스 위치·방향에 마무리 범위를 고정 표시 (돌진 경로 표시는 교체됨)
	float ImpactTime = 0.f;

	if (FindMontageEventTime(FinishMontage, MontageImpactEventTag, ImpactTime))
	{
		BeginMontageAreaTelegraphAtTime(FinishMontage, ImpactTime, FinishAreaData, nullptr, false);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossChargeSmash] %s: notify with %s not found in %s."),
			*GetName(), *MontageImpactEventTag.ToString(), *FinishMontage->GetName());
		ClearAreaTelegraph();
	}

	UAbilityTask_WaitGameplayEvent* ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, MontageImpactEventTag, nullptr, true, true);
	ImpactTask->EventReceived.AddDynamic(this, &ThisClass::HandleFinishImpact);
	ImpactTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FinishMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleFinishMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleFinishMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleFinishMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleFinishMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_BossChargeSmash::HandleFinishImpact(FGameplayEventData Payload)
{
	const FGameplayEffectSpecHandle FinishSpecHandle = FinishDamageEffectClass
		? MakeAIDamageEffectSpecHandle(FinishDamageEffectClass, FinishDamageScalableFloat)
		: FGameplayEffectSpecHandle();

	ApplyAreaDamage(FinishSpecHandle, true);
}

void UAIGameplayAbility_BossChargeSmash::HandleFinishMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_BossChargeSmash::HandleFinishMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_BossChargeSmash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopLoopMontages();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_BossChargeSmash::PlayLoopMontage(UAnimMontage* Montage) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = OwnerCharacter && OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;

	if (AnimInstance && Montage)
	{
		AnimInstance->Montage_Play(Montage);
	}
}

void UAIGameplayAbility_BossChargeSmash::StopLoopMontages() const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = OwnerCharacter && OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;

	if (!AnimInstance)
	{
		return;
	}

	for (UAnimMontage* Montage : { ChargeMontage.Get(), DashMontage.Get() })
	{
		if (Montage && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(0.15f, Montage);
		}
	}
}
