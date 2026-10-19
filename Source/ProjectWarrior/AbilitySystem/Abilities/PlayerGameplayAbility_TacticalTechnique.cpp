// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_TacticalTechnique.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"

const FName UPlayerGameplayAbility_TacticalTechnique::WarpTargetName = TEXT("TacticalTarget");

UPlayerGameplayAbility_TacticalTechnique::UPlayerGameplayAbility_TacticalTechnique()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Player.Ability.Attack은 넣지 않음(좌클릭 공격이 이 기술을 발동하면 안 됨)
	FGameplayTagContainer TechniqueAssetTags;
	TechniqueAssetTags.AddTag(WarriorGameplayTags::Player_Ability_TacticalTechnique);
	SetAssetTags(TechniqueAssetTags);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);

	AttackTypeTag = WarriorGameplayTags::Player_SetByCaller_AttackType_Heavy;
	HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_Heavy;
}

void UPlayerGameplayAbility_TacticalTechnique::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* OwnerCharacter = GetPlayerCharacterFromActorInfo();
	const AActor* Target = TriggerEventData ? TriggerEventData->Target.Get() : nullptr;

	if (!OwnerCharacter || !Target || !TechniqueMontage)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[TacticalTechnique] %s: missing owner, target or montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 스태미나 비용. 모자라면 실행하지 않음
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[TacticalTechnique] %s: commit failed (stamina?)."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 대상 캡슐 표면에서 StopDistance 앞, 대상을 바라보는 자리로 대시
	const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector ToTarget = TargetLocation - OwnerLocation;
	ToTarget.Z = 0.0f;
	const FVector Direction = ToTarget.GetSafeNormal(UE_SMALL_NUMBER, OwnerCharacter->GetActorForwardVector());

	const UCapsuleComponent* TargetCapsule = Target->FindComponentByClass<UCapsuleComponent>();
	const float TargetRadius = TargetCapsule ? TargetCapsule->GetScaledCapsuleRadius() : 50.0f;
	const float OwnerRadius = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float StopOffset = FMath::Min(TargetRadius + OwnerRadius + StopDistance, ToTarget.Size());

	FVector WarpLocation = TargetLocation - Direction * StopOffset;
	WarpLocation.Z = OwnerLocation.Z;
	const FRotator FaceRotation = Direction.Rotation();

	OwnerCharacter->SetActorRotation(FaceRotation);

	UMotionWarpingComponent* MotionWarpingComponent = OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>();

	if (MotionWarpingComponent)
	{
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, WarpLocation, FaceRotation);
	}

	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WarriorGameplayTags::Shared_Event_MeleeHit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHit);
	HitTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TechniqueMontage, PlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();

	// 재생 중인 몽타주에 워핑 구간을 붙임(수정자는 재생 중인 루트 모션 몽타주와 같아야 동작)
	if (MotionWarpingComponent)
	{
		const float EndTime = WarpEndTime > 0.0f ? WarpEndTime : TechniqueMontage->GetPlayLength();

		URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(MotionWarpingComponent, TechniqueMontage, WarpStartTime, FMath::Max(EndTime, WarpStartTime + 0.05f),
			WarpTargetName, EWarpPointAnimProvider::None, FTransform::Identity, NAME_None, true, true, true);
	}
}

void UPlayerGameplayAbility_TacticalTechnique::OnMeleeHit(FGameplayEventData Payload)
{
	AActor* HitActor = const_cast<AActor*>(Payload.Target.Get());

	if (!HitActor || !DamageEffectClass)
	{
		return;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = MakePlayerDamageEffectSpecHandle(DamageEffectClass, BaseDamage, AttackTypeTag, 1, FGameplayTag());

	if (DamageSpecHandle.IsValid() && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
	{
		NativeApplyEffectSpecHandleToTarget(HitActor, DamageSpecHandle);
	}

	if (HitReactEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.Instigator = GetAvatarActorFromActorInfo();
		EventData.Target = HitActor;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitActor, HitReactEventTag, EventData);
	}
}

void UPlayerGameplayAbility_TacticalTechnique::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerGameplayAbility_TacticalTechnique::OnMontageInterrupted()
{
	UE_LOG(LogProjectWarrior, Warning, TEXT("[TacticalTechnique] %s: montage interrupted or cancelled."), *GetName());

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
