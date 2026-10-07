// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_Snipe.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "ProjectWarrior/Items/WarriorAimBeam.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/ProjectWarrior.h"

UAIGameplayAbility_Snipe::UAIGameplayAbility_Snipe()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer SnipeAssetTags;
	SnipeAssetTags.AddTag(WarriorGameplayTags::AI_Ability_Special_Snipe);
	SetAssetTags(SnipeAssetTags);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Finisher);

	// 고정한 뒤에는 회피 여부와 관계없이 고정 위치로 발사 (조준선이 보여 준 곳 = 화살이 가는 곳)
	bOnlyUseLockedAimWhenDodged = false;

	FireEventTag = WarriorGameplayTags::AI_Event_Projectile_Fire;
}

void UAIGameplayAbility_Snipe::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	SnipeTarget = FindAITargetActor(ActorInfo);
	bFired = false;
	bAimLocked = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !SnipeMontage || !ProjectileClass || !SnipeTarget.IsValid())
	{
		UE_CLOG(!SnipeMontage || !ProjectileClass, LogProjectWarrior, Warning, TEXT("[Snipe] %s needs SnipeMontage and ProjectileClass."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WatchForFinisherOrDeath();

	if (const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AAIController* AIController = Cast<AAIController>(AvatarPawn->GetController()))
		{
			AIController->StopMovement();
			AIController->SetFocus(SnipeTarget.Get());
		}
	}

	SpawnHeldProjectile(ProjectileClass, ProjectileSocketName, bAttachProjectileToCharacterMesh);

	if (AimBeamClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetAvatarActorFromActorInfo();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AWarriorAimBeam* Beam = GetWorld()->SpawnActor<AWarriorAimBeam>(AimBeamClass, GetAvatarActorFromActorInfo()->GetActorTransform(), SpawnParams))
		{
			Beam->InitializeBeam(GetProjectileSocketParent(bAttachProjectileToCharacterMesh), AimBeamSocketName.IsNone() ? ProjectileSocketName : AimBeamSocketName, SnipeTarget.Get());
			AimBeam = Beam;
		}
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SnipeMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnSnipeMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnSnipeMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnSnipeMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnSnipeMontageInterrupted);
	MontageTask->ReadyForActivation();

	// 발사 구간으로 넘어갈 때까지 조준 구간을 반복
	if (SnipeMontage->IsValidSectionName(AimSection))
	{
		MontageSetNextSectionName(AimSection, AimSection);
	}

	UAbilityTask_WaitDelay* LockTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(AimDuration - LockLeadTime, 0.f));
	LockTask->OnFinish.AddDynamic(this, &ThisClass::OnLockAim);
	LockTask->ReadyForActivation();

	UAbilityTask_WaitDelay* FireTask = UAbilityTask_WaitDelay::WaitDelay(this, AimDuration);
	FireTask->OnFinish.AddDynamic(this, &ThisClass::OnStartFire);
	FireTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* FireEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag, nullptr, true, true);
	FireEventTask->EventReceived.AddDynamic(this, &ThisClass::OnFireEvent);
	FireEventTask->ReadyForActivation();
}

void UAIGameplayAbility_Snipe::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	DestroyAimBeam();
	SnipeTarget.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_Snipe::OnLockAim()
{
	// 한 번만 고정 (발사 시점에 다시 고정하면 고정 후 피한 플레이어를 따라가 버림)
	if (bAimLocked)
	{
		return;
	}
	bAimLocked = true;

	LockAimLocation(SnipeTarget.Get());

	if (AWarriorAimBeam* Beam = AimBeam.Get())
	{
		Beam->LockBeam();
	}
}

void UAIGameplayAbility_Snipe::OnStartFire()
{
	// LockLeadTime이 0이면 아직 고정 전이므로 여기서 고정
	OnLockAim();

	if (SnipeMontage->IsValidSectionName(FireSection))
	{
		MontageJumpToSection(FireSection);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Snipe] %s: SnipeMontage has no '%s' section. Firing immediately."), *GetName(), *FireSection.ToString());
		OnFireEvent(FGameplayEventData());
	}
}

void UAIGameplayAbility_Snipe::OnFireEvent(FGameplayEventData Payload)
{
	if (bFired)
	{
		return;
	}
	bFired = true;

	DestroyAimBeam();

	FGameplayEffectSpecHandle DamageSpecHandle;
	if (SnipeDamageEffect)
	{
		DamageSpecHandle = MakeAIDamageEffectSpecHandle(SnipeDamageEffect, SnipeDamage);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Snipe] %s has no SnipeDamageEffect."), *GetName());
	}

	LaunchHeldProjectile(SnipeTarget.Get(), DamageSpecHandle, false);
}

void UAIGameplayAbility_Snipe::OnSnipeMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_Snipe::OnSnipeMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_Snipe::DestroyAimBeam()
{
	if (AWarriorAimBeam* Beam = AimBeam.Get())
	{
		Beam->Destroy();
	}

	AimBeam.Reset();
}
