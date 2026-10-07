// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_Knockdown.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Animation/AnimMontage.h"
#include "Character/ALSBaseCharacter.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/ProjectWarrior.h"

UPlayerGameplayAbility_Knockdown::UPlayerGameplayAbility_Knockdown()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer KnockdownAssetTags;
	KnockdownAssetTags.AddTag(WarriorGameplayTags::Shared_Ability_HitReact_Knockdown);
	SetAssetTags(KnockdownAssetTags);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = WarriorGameplayTags::Shared_Event_HitReact_KnockBack;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	// 기존 피격 경직과 같은 상태 태그
	ActivationOwnedTags.AddTag(WarriorGameplayTags::Shared_Status_HitReact);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);

	// 넘어지는 순간 하던 행동은 끊고, 넘어져 있는 동안 다른 행동·피격 경직이 끼어들지 않게 함
	const FGameplayTag InterruptedAbilityTags[] = {
		WarriorGameplayTags::Player_Ability_Attack,
		WarriorGameplayTags::Player_Ability_Block,
		WarriorGameplayTags::Player_Ability_Dodge,
		WarriorGameplayTags::Player_Ability_DodgeRoll,
		WarriorGameplayTags::Player_Ability_Sprint,
		WarriorGameplayTags::Player_Ability_Counter,
		WarriorGameplayTags::Player_Ability_Equip_Weapon_Katana,
		WarriorGameplayTags::Player_Ability_Unequip_Weapon_Katana,
		WarriorGameplayTags::Shared_Ability_HitReact };
	for (const FGameplayTag& Tag : InterruptedAbilityTags)
	{
		CancelAbilitiesWithTag.AddTag(Tag);
		BlockAbilitiesWithTag.AddTag(Tag);
	}
}

void UPlayerGameplayAbility_Knockdown::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AActor* Attacker = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;

	// 공격자가 앞쪽(정면 기준 ±90도)이면 앞에서 맞은 것
	bool bHitFromFront = true;
	FVector ToAttacker = FVector::ZeroVector;
	if (Avatar && Attacker)
	{
		ToAttacker = (Attacker->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		bHitFromFront = ToAttacker.IsNearlyZero() || FVector::DotProduct(Avatar->GetActorForwardVector(), ToAttacker) >= 0.f;
	}

	UAnimMontage* Montage = bHitFromFront ? FrontHitMontage : BackHitMontage;
	if (!Montage)
	{
		Montage = bHitFromFront ? BackHitMontage : FrontHitMontage;
	}

	if (!Montage)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Knockdown] %s has no knockdown montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 공격자 반대 방향으로 넘어지도록 몸을 맞춤 (앞에서 맞으면 공격자를 정면에, 뒤에서 맞으면 등 뒤에)
	if (bAlignToAttacker && !ToAttacker.IsNearlyZero())
	{
		FaceDirection(bHitFromFront ? ToAttacker : -ToAttacker);
	}

	PlayingMontage = Montage;

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, Montage->IsValidSectionName(StartSection) ? StartSection : NAME_None);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnKnockdownMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnKnockdownMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnKnockdownMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnKnockdownMontageInterrupted);
	MontageTask->ReadyForActivation();

	// 누운 상태 구간은 이 재생에서만 반복 (에셋의 구간 연결과 무관)
	const bool bHasLoop = Montage->IsValidSectionName(LoopSection);
	if (bHasLoop)
	{
		MontageSetNextSectionName(LoopSection, LoopSection);
	}

	if (bInvulnerableWhileFalling)
	{
		SetInvulnerable(true);
	}

	// 사망하면 넘어진 상태를 정리 (사망 어빌리티가 몽타주를 재생)
	UAbilityTask_WaitGameplayTagAdded* DeathTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, WarriorGameplayTags::Shared_Status_Death, nullptr, true);
	DeathTask->Added.AddDynamic(this, &ThisClass::OnOwnerDied);
	DeathTask->ReadyForActivation();

	const float StartLength = Montage->IsValidSectionName(StartSection)
		? Montage->GetSectionLength(Montage->GetSectionIndex(StartSection))
		: 0.f;

	UAbilityTask_WaitDelay* DownTask = UAbilityTask_WaitDelay::WaitDelay(this, StartLength);
	DownTask->OnFinish.AddDynamic(this, &ThisClass::OnDown);
	DownTask->ReadyForActivation();

	// 반복 구간이 없으면 몽타주가 끝날 때까지 그대로 재생
	if (bHasLoop)
	{
		UAbilityTask_WaitDelay* GetUpTask = UAbilityTask_WaitDelay::WaitDelay(this, StartLength + DownDuration);
		GetUpTask->OnFinish.AddDynamic(this, &ThisClass::OnGetUp);
		GetUpTask->ReadyForActivation();
	}
}

void UPlayerGameplayAbility_Knockdown::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetInvulnerable(false);
	PlayingMontage.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerGameplayAbility_Knockdown::OnDown()
{
	SetInvulnerable(true);
}

void UPlayerGameplayAbility_Knockdown::OnGetUp()
{
	const UAnimMontage* Montage = PlayingMontage.Get();
	if (Montage && Montage->IsValidSectionName(EndSection))
	{
		MontageJumpToSection(EndSection);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Knockdown] %s: montage has no '%s' section."), *GetName(), *EndSection.ToString());
		MontageStop(0.25f);
	}

	if (!bInvulnerableWhileGettingUp)
	{
		SetInvulnerable(false);
	}
}

void UPlayerGameplayAbility_Knockdown::OnKnockdownMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerGameplayAbility_Knockdown::OnKnockdownMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPlayerGameplayAbility_Knockdown::OnOwnerDied()
{
	if (IsActive())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UPlayerGameplayAbility_Knockdown::SetInvulnerable(bool bEnable)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bInvulnerable == bEnable)
	{
		return;
	}

	bInvulnerable = bEnable;

	if (bEnable)
	{
		ASC->AddLooseGameplayTag(WarriorGameplayTags::Shared_Status_Invulnerable);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(WarriorGameplayTags::Shared_Status_Invulnerable);
	}
}

void UPlayerGameplayAbility_Knockdown::FaceDirection(const FVector& Direction) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || Direction.IsNearlyZero())
	{
		return;
	}

	const FRotator NewRotation(0.f, Direction.Rotation().Yaw, 0.f);

	// ALS는 목표 회전(TargetRotation)으로 되돌리려 하므로 함께 갱신. 카메라(컨트롤 회전)는 건드리지 않음
	if (AALSBaseCharacter* ALSCharacter = Cast<AALSBaseCharacter>(Avatar))
	{
		ALSCharacter->SetActorLocationAndTargetRotation(ALSCharacter->GetActorLocation(), NewRotation);
	}
	else
	{
		Avatar->SetActorRotation(NewRotation);
	}
}
