// Fill out your copyright notice in the Description page of Project Settings.

#include "AIGameplayAbility_Stun.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Pawn.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"

namespace
{
	const FName StunLogicReason(TEXT("Stunned"));
}

UAIGameplayAbility_Stun::UAIGameplayAbility_Stun()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer StunAssetTags;
	StunAssetTags.AddTag(WarriorGameplayTags::Shared_Ability_Stagger);
	SetAssetTags(StunAssetTags);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = WarriorGameplayTags::Shared_Event_Stagger;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	ActivationOwnedTags.AddTag(WarriorGameplayTags::Shared_Status_Stunned);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);

	//처형(AI.Ability.Finisher)은 막지 않는다. 기절한 적은 처형할 수 있어야 한다
	const TArray<FGameplayTag> StoppedAbilities = {
		WarriorGameplayTags::AI_Ability_Melee, WarriorGameplayTags::AI_Ability_Range, WarriorGameplayTags::AI_Ability_Block,
		WarriorGameplayTags::AI_Ability_Dodge, WarriorGameplayTags::AI_Ability_Special, WarriorGameplayTags::AI_Ability_Boss,
		WarriorGameplayTags::Shared_Ability_HitReact,
		WarriorGameplayTags::AI_Ability_Equip_Weapon_Katana, WarriorGameplayTags::AI_Ability_Unequip_Weapon_Katana,
		WarriorGameplayTags::AI_Ability_Equip_Weapon_Bow, WarriorGameplayTags::AI_Ability_Unequip_Weapon_Bow };
	for (const FGameplayTag& Tag : StoppedAbilities)
	{
		CancelAbilitiesWithTag.AddTag(Tag);
		BlockAbilitiesWithTag.AddTag(Tag);
	}
}

void UAIGameplayAbility_Stun::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAnimMontage* Montage = StunMontages.IsEmpty() ? nullptr : StunMontages[FMath::RandRange(0, StunMontages.Num() - 1)].Get();
	if (!Montage)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stun] %s has no StunMontages. Ending."), *GetNameSafe(this));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SetBehaviorPaused(true);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, StunPlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stun] %s stunned (%s, %.1fs)."),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *Montage->GetName(), Montage->GetPlayLength() / FMath::Max(StunPlayRate, 0.1f));
}

void UAIGameplayAbility_Stun::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bRestoreBalanceOnEnd)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			const float MaxBalance = ASC->GetNumericAttribute(UWarriorAttributeSet::GetMaxBalanceAttribute());
			ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetCurrentBalanceAttribute(), MaxBalance);
		}
	}

	SetBehaviorPaused(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_Stun::HandleMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_Stun::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_Stun::SetBehaviorPaused(bool bInPaused) const
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	if (!AIController)
	{
		return;
	}

	if (bInPaused)
	{
		AIController->StopMovement();
	}

	//BT가 멈춰 있는 허수아비(튜토리얼)는 Pause·Resume 모두 영향이 없다
	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		if (bInPaused)
		{
			Brain->PauseLogic(StunLogicReason.ToString());
		}
		else
		{
			Brain->ResumeLogic(StunLogicReason.ToString());
		}
	}
}
