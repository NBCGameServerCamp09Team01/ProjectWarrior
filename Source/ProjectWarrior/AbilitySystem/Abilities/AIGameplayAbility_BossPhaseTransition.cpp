// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossPhaseTransition.h"
#include "ProjectWarrior/Characters/WarriorBossCharacter.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AIController.h"
#include "BrainComponent.h"

UAIGameplayAbility_BossPhaseTransition::UAIGameplayAbility_BossPhaseTransition()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer TransitionAssetTags;
	TransitionAssetTags.AddTag(WarriorGameplayTags::AI_Ability_Boss_PhaseTransition);
	SetAssetTags(TransitionAssetTags);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = WarriorGameplayTags::AI_Event_Boss_PhaseChanged;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	// 연출 중에는 맞지 않고(무적) 경직도 없음(슈퍼아머)
	ActivationOwnedTags.AddTag(WarriorGameplayTags::Shared_Status_Invulnerable);
	ActivationOwnedTags.AddTag(WarriorGameplayTags::AI_Status_SuperArmor);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);

	// 진행 중인 패턴(도약 중 포함)·경직·스태거를 끊고, 연출 동안 다시 시작하지 않게 함
	const FGameplayTag InterruptedAbilityTags[] = {
		WarriorGameplayTags::AI_Ability_Boss,
		WarriorGameplayTags::AI_Ability_Melee,
		WarriorGameplayTags::AI_Ability_Range,
		WarriorGameplayTags::AI_Ability_Special,
		WarriorGameplayTags::AI_Ability_Block,
		WarriorGameplayTags::AI_Ability_Dodge,
		WarriorGameplayTags::Shared_Ability_HitReact,
		WarriorGameplayTags::Shared_Ability_Stagger };
	for (const FGameplayTag& Tag : InterruptedAbilityTags)
	{
		CancelAbilitiesWithTag.AddTag(Tag);
		BlockAbilitiesWithTag.AddTag(Tag);
	}

	// 충격파는 넘어뜨리기만 함 (피해 없음)
	HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_KnockBack;
}

void UAIGameplayAbility_BossPhaseTransition::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AWarriorBossCharacter* BossCharacter = Cast<AWarriorBossCharacter>(GetAvatarActorFromActorInfo());
	const int32 NewPhase = TriggerEventData && TriggerEventData->EventMagnitude > 0.f
		? FMath::RoundToInt(TriggerEventData->EventMagnitude)
		: (BossCharacter ? BossCharacter->GetCurrentPhase() : 1);

	ApplyPhaseEffect(NewPhase);

	if (!TransitionMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (bPauseBehaviorTree)
	{
		SetBehaviorTreePaused(true);
	}

	// 그로기 중에 전환되면 충격파 없음 (AWarriorBossCharacter가 스태거 중이었으면 표시해서 보냄)
	const bool bWasStaggered = TriggerEventData && TriggerEventData->InstigatorTags.HasTagExact(WarriorGameplayTags::Shared_Ability_Stagger);

	if (bApplyShockwave && !bWasStaggered)
	{
		float TelegraphTime = 0.f;

		if (FindMontageEventTime(TransitionMontage, WarriorGameplayTags::AI_Event_Boss_Telegraph, TelegraphTime))
		{
			// 쉬는 구간에는 표시 없이, 표시 노티파이(포효 시작)부터 판정 시점까지만 표시
			UAbilityTask_WaitGameplayEvent* TelegraphTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WarriorGameplayTags::AI_Event_Boss_Telegraph, nullptr, true, true);
			TelegraphTask->EventReceived.AddDynamic(this, &ThisClass::HandleTelegraphEvent);
			TelegraphTask->ReadyForActivation();
		}
		// 표시 노티파이가 없으면 시작부터 판정 시점까지 표시
		else if (!BeginMontageAreaTelegraph(TransitionMontage, nullptr, false))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[BossPhaseTransition] %s: montage has no '%s' notify. Shockwave skipped."),
				*GetName(), *MontageImpactEventTag.ToString());
		}

		UAbilityTask_WaitGameplayEvent* ShockwaveTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, MontageImpactEventTag, nullptr, true, true);
		ShockwaveTask->EventReceived.AddDynamic(this, &ThisClass::HandleShockwaveEvent);
		ShockwaveTask->ReadyForActivation();
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TransitionMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_BossPhaseTransition::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetBehaviorTreePaused(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_BossPhaseTransition::HandleTelegraphEvent(FGameplayEventData Payload)
{
	// 재생 중에 호출하면 현재 위치부터 판정 시점까지 남은 시간으로 표시
	BeginMontageAreaTelegraph(TransitionMontage, nullptr, false);
}

void UAIGameplayAbility_BossPhaseTransition::HandleShockwaveEvent(FGameplayEventData Payload)
{
	// 피해 효과 없이 판정 -> 맞은 대상에게 HitReactEventTag(KnockBack)만 보냄
	ApplyAreaDamage(FGameplayEffectSpecHandle());
}

void UAIGameplayAbility_BossPhaseTransition::HandleMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_BossPhaseTransition::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_BossPhaseTransition::ApplyPhaseEffect(int32 NewPhase)
{
	const TSubclassOf<UGameplayEffect>* EffectClass = PhaseEffects.Find(NewPhase);

	if (EffectClass && *EffectClass)
	{
		ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, (*EffectClass)->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());
	}
}

void UAIGameplayAbility_BossPhaseTransition::SetBehaviorTreePaused(bool bPaused)
{
	if (bPausedBehaviorTree == bPaused)
	{
		return;
	}

	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	AAIController* AIController = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;
	UBrainComponent* BrainComponent = AIController ? AIController->GetBrainComponent() : nullptr;

	if (!BrainComponent)
	{
		return;
	}

	if (bPaused)
	{
		AIController->StopMovement();
		BrainComponent->PauseLogic(TEXT("BossPhaseTransition"));
	}
	else
	{
		// 연출 중 사망 등으로 로직이 이미 멈췄으면 다시 켜지 않음
		if (BrainComponent->IsPaused())
		{
			BrainComponent->ResumeLogic(TEXT("BossPhaseTransition"));
		}
	}

	bPausedBehaviorTree = bPaused;
}
