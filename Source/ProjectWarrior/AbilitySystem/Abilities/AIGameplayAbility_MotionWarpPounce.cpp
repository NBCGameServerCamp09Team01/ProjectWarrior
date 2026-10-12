// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_MotionWarpPounce.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"

UAIGameplayAbility_MotionWarpPounce::UAIGameplayAbility_MotionWarpPounce()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 지면을 따라 달려드는 공격 기준 (점프 높이가 루트에 없는 애니메이션)
	bUseFlyingDuringLeap = false;
	bSetALSInAirDuringLeap = false;
	bCenterAreaOnLanding = true;
	LandingGap = 20.f;
}

void UAIGameplayAbility_MotionWarpPounce::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
		UE_LOG(LogProjectWarrior, Warning, TEXT("[MotionWarpPounce] %s has no attack montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	float ImpactTime = 0.f;
	const bool bHasImpact = FindMontageEventTime(AttackMontage, MontageImpactEventTag, ImpactTime);

	if (!bHasImpact)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[MotionWarpPounce] %s: notify with %s not found in %s."),
			*GetName(), *MontageImpactEventTag.ToString(), *AttackMontage->GetName());
	}

	const float PlayRate = FMath::Max(AttackPlayRate, 0.1f);
	const float TelegraphDuration = bHasImpact ? ImpactTime / PlayRate : 0.3f;

	// 착지 지점·Warp 목표·위험 범위를 이 시점의 대상 위치로 확정
	if (!BeginLeap(nullptr, TelegraphDuration))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BeginLeapMovement();

	UAbilityTask_WaitGameplayEvent* ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, MontageImpactEventTag, nullptr, true, true);
	ImpactTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactEvent);
	ImpactTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, PlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	// 재생 중인 몽타주에 Warp 구간 추가 (수정자는 재생 중인 루트 모션 몽타주와 같아야 동작)
	if (UMotionWarpingComponent* MotionWarpingComponent = GetAvatarActorFromActorInfo()->FindComponentByClass<UMotionWarpingComponent>())
	{
		const float EndTime = WarpEndTime > 0.f ? WarpEndTime : (bHasImpact ? ImpactTime : AttackMontage->GetPlayLength());

		URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(MotionWarpingComponent, AttackMontage, WarpStartTime, FMath::Max(EndTime, WarpStartTime + 0.05f),
			WarpTargetName, EWarpPointAnimProvider::None, FTransform::Identity, NAME_None, true, true, true);
	}
}

void UAIGameplayAbility_MotionWarpPounce::HandleImpactEvent(FGameplayEventData Payload)
{
	EndLeapMovement();

	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	ApplyAreaDamage(DamageSpecHandle, true);
}

void UAIGameplayAbility_MotionWarpPounce::HandleMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_MotionWarpPounce::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
