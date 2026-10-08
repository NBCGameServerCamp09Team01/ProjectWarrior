// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_GuardCounter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AIController.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/ProjectWarrior.h"

namespace
{
	enum class EMontageEndEvents : uint8
	{
		// 끝까지 재생됨 (블렌드 아웃 후)
		Completed,
		// 블렌드 아웃 시작 (끊기지 않은 경우)
		BlendOut,
		// 어떻게 끝나든 (블렌드 아웃·완료·끊김·취소). 한 번 이상 호출될 수 있으므로 EndAbility처럼 여러 번 불려도 되는 함수에만 사용
		Any
	};

	// 끝날 때 FinishedFunction을 호출하는 몽타주 태스크. 블렌드 아웃과 완료가 둘 다 오므로 다시 재생하는 콜백은 하나만 받아야 함
	UAbilityTask_PlayMontageAndWait* PlayMontageTask(UGameplayAbility* Ability, UAnimMontage* Montage, UObject* Listener, FName FinishedFunction, EMontageEndEvents EndEvents, FName StartSection = NAME_None)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(Ability, NAME_None, Montage, 1.f, StartSection);

		FScriptDelegate Delegate;
		Delegate.BindUFunction(Listener, FinishedFunction);
		switch (EndEvents)
		{
		case EMontageEndEvents::Completed:
			Task->OnCompleted.Add(Delegate);
			break;
		case EMontageEndEvents::BlendOut:
			Task->OnBlendOut.Add(Delegate);
			break;
		case EMontageEndEvents::Any:
			Task->OnCompleted.Add(Delegate);
			Task->OnBlendOut.Add(Delegate);
			Task->OnInterrupted.Add(Delegate);
			Task->OnCancelled.Add(Delegate);
			break;
		}

		Task->ReadyForActivation();
		return Task;
	}
}

UAIGameplayAbility_GuardCounter::UAIGameplayAbility_GuardCounter()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer GuardAssetTags;
	GuardAssetTags.AddTag(WarriorGameplayTags::AI_Ability_Block_Counter);
	SetAssetTags(GuardAssetTags);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Finisher);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::AI_Status_Dodging);
}

void UAIGameplayAbility_GuardCounter::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !GuardMontage || IsOwnerIncapacitated(GetAbilitySystemComponentFromActorInfo()))
	{
		UE_CLOG(!GuardMontage, LogProjectWarrior, Warning, TEXT("[GuardCounter] %s has no GuardMontage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Phase = EGuardPhase::Guarding;
	bPlayingGuardHit = false;
	bCounterScheduled = false;

	// 가드·반격 도중 처형이나 사망이 시작되면 취소
	WatchForFinisherOrDeath();

	if (const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AAIController* AIController = Cast<AAIController>(AvatarPawn->GetController()))
		{
			AIController->StopMovement();
		}
	}
	FaceAreaTarget();

	SetOwnerTag(WarriorGameplayTags::AI_Status_Guarding, true, bGuardingTag);
	PlayGuardMontage();

	UAbilityTask_WaitGameplayEvent* GuardHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WarriorGameplayTags::AI_Event_GuardHit, nullptr, false, true);
	GuardHitTask->EventReceived.AddDynamic(this, &ThisClass::OnGuardHit);
	GuardHitTask->ReadyForActivation();

	UAbilityTask_WaitDelay* TimeoutTask = UAbilityTask_WaitDelay::WaitDelay(this, GuardDuration);
	TimeoutTask->OnFinish.AddDynamic(this, &ThisClass::OnGuardTimeout);
	TimeoutTask->ReadyForActivation();
	GuardTimeoutTask = TimeoutTask;
}

void UAIGameplayAbility_GuardCounter::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Phase = EGuardPhase::None;
	GuardTimeoutTask = nullptr;
	SetOwnerTag(WarriorGameplayTags::AI_Status_Guarding, false, bGuardingTag);
	SetOwnerTag(WarriorGameplayTags::AI_Status_SuperArmor, false, bSuperArmorTag);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_GuardCounter::PlayGuardMontage(FName StartSection)
{
	if (!StartSection.IsNone() && !GuardMontage->IsValidSectionName(StartSection))
	{
		StartSection = NAME_None;
	}

	PlayMontageTask(this, GuardMontage, this, GET_FUNCTION_NAME_CHECKED(ThisClass, OnGuardMontageEnded), EMontageEndEvents::Completed, StartSection);

	// 가드 유지 구간을 이 재생에서만 반복 (에셋의 구간 연결은 바꾸지 않으므로 같은 몽타주를 쓰는 플레이어에 영향 없음)
	if (!GuardResumeSection.IsNone() && GuardMontage->IsValidSectionName(GuardResumeSection))
	{
		MontageSetNextSectionName(GuardResumeSection, GuardResumeSection);
	}
}

void UAIGameplayAbility_GuardCounter::OnGuardMontageEnded()
{
	// 가드 해제 구간까지 끝나면 종료
	if (Phase == EGuardPhase::Ending)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 반복 구간이 없는 몽타주가 끝나면 가드 중에 다시 재생 (끊긴 경우에는 호출되지 않음)
	if (Phase == EGuardPhase::Guarding && !bPlayingGuardHit)
	{
		PlayGuardMontage(GuardResumeSection);
	}
}

void UAIGameplayAbility_GuardCounter::OnGuardHit(FGameplayEventData Payload)
{
	if (Phase != EGuardPhase::Guarding)
	{
		return;
	}

	if (!bPlayingGuardHit && !GuardHitMontages.IsEmpty())
	{
		if (UAnimMontage* HitMontage = GuardHitMontages[FMath::RandRange(0, GuardHitMontages.Num() - 1)])
		{
			bPlayingGuardHit = true;
			PlayMontageTask(this, HitMontage, this, GET_FUNCTION_NAME_CHECKED(ThisClass, OnGuardHitMontageEnded), EMontageEndEvents::BlendOut);
		}
	}

	if (!bCounterScheduled)
	{
		bCounterScheduled = true;

		if (GuardTimeoutTask)
		{
			GuardTimeoutTask->EndTask();
			GuardTimeoutTask = nullptr;
		}

		UAbilityTask_WaitDelay* CounterDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CounterDelay);
		CounterDelayTask->OnFinish.AddDynamic(this, &ThisClass::StartCounter);
		CounterDelayTask->ReadyForActivation();
	}
}

void UAIGameplayAbility_GuardCounter::OnGuardHitMontageEnded()
{
	bPlayingGuardHit = false;

	if (Phase == EGuardPhase::Guarding)
	{
		PlayGuardMontage(GuardResumeSection);
	}
}

void UAIGameplayAbility_GuardCounter::OnGuardTimeout()
{
	if (Phase != EGuardPhase::Guarding)
	{
		return;
	}

	Phase = EGuardPhase::Ending;
	GuardTimeoutTask = nullptr;
	SetOwnerTag(WarriorGameplayTags::AI_Status_Guarding, false, bGuardingTag);

	// 가드 몽타주의 해제 구간으로 넘어가 끝까지 재생 -> OnGuardMontageEnded에서 종료
	if (!GuardEndSection.IsNone() && GuardMontage->IsValidSectionName(GuardEndSection))
	{
		MontageJumpToSection(GuardEndSection);

		// 해제 구간이 다른 몽타주에 끊겨 완료 콜백이 오지 않아도 끝나도록
		const float SectionLength = GuardMontage->GetSectionLength(GuardMontage->GetSectionIndex(GuardEndSection));
		UAbilityTask_WaitDelay* EndFallbackTask = UAbilityTask_WaitDelay::WaitDelay(this, SectionLength + 0.5f);
		EndFallbackTask->OnFinish.AddDynamic(this, &ThisClass::OnFinalMontageEnded);
		EndFallbackTask->ReadyForActivation();
		return;
	}

	if (GuardEndMontage)
	{
		PlayMontageTask(this, GuardEndMontage, this, GET_FUNCTION_NAME_CHECKED(ThisClass, OnFinalMontageEnded), EMontageEndEvents::Any);
		return;
	}

	MontageStop(0.25f);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_GuardCounter::StartCounter()
{
	if (Phase != EGuardPhase::Guarding)
	{
		return;
	}

	Phase = EGuardPhase::Countering;
	SetOwnerTag(WarriorGameplayTags::AI_Status_Guarding, false, bGuardingTag);

	if (!CounterMontage)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[GuardCounter] %s has no CounterMontage."), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (bSuperArmorDuringCounter)
	{
		SetOwnerTag(WarriorGameplayTags::AI_Status_SuperArmor, true, bSuperArmorTag);
	}

	// 판정 시점의 위치·방향에 위험 범위를 미리 표시 (대상 쪽으로 돌아선 뒤 계산)
	BeginMontageAreaTelegraph(CounterMontage, nullptr, true);

	UAbilityTask_WaitGameplayEvent* ImpactTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, MontageImpactEventTag, nullptr, true, true);
	ImpactTask->EventReceived.AddDynamic(this, &ThisClass::OnCounterImpact);
	ImpactTask->ReadyForActivation();

	PlayMontageTask(this, CounterMontage, this, GET_FUNCTION_NAME_CHECKED(ThisClass, OnFinalMontageEnded), EMontageEndEvents::Any);
}

void UAIGameplayAbility_GuardCounter::OnCounterImpact(FGameplayEventData Payload)
{
	if (!CounterDamageEffect)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[GuardCounter] %s has no CounterDamageEffect."), *GetName());
		ClearAreaTelegraph();
		return;
	}

	ApplyAreaDamage(MakeAIDamageEffectSpecHandle(CounterDamageEffect, CounterDamage));
}

void UAIGameplayAbility_GuardCounter::OnFinalMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_GuardCounter::SetOwnerTag(const FGameplayTag& Tag, bool bEnable, bool& bState)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bState == bEnable)
	{
		return;
	}

	bState = bEnable;

	if (bEnable)
	{
		ASC->AddLooseGameplayTag(Tag);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}
}
