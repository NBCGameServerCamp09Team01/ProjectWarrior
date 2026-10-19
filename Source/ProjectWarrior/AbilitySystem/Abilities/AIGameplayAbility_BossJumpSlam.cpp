// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossJumpSlam.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionJumpForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UAIGameplayAbility_BossJumpSlam::UAIGameplayAbility_BossJumpSlam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	bCenterAreaOnLanding = true;
}

void UAIGameplayAbility_BossJumpSlam::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	if (!LeapMontage || !GetOwnerAnimInstance())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossJumpSlam] %s needs LeapMontage and a character mesh."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	LeapIndex = 0;
	SavedAreaRadius = DefaultAreaData.Radius;
	bRadiusOverridden = false;

	StartLeap();
}

void UAIGameplayAbility_BossJumpSlam::StartLeap()
{
	const float Rate = FMath::Max(PlayRate, 0.1f);

	if (LandingRadii.IsValidIndex(LeapIndex))
	{
		DefaultAreaData.Radius = LandingRadii[LeapIndex];
		bRadiusOverridden = true;
	}

	// 이륙 전 준비 + 공중 + 착지 판정까지 위험 범위가 차오름
	const float TelegraphDuration = TakeoffTime / Rate + AirDuration + LandImpactDelay / Rate;

	FaceAreaTarget();

	if (!BeginLeap(nullptr, TelegraphDuration))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	AnimInstance->Montage_Play(LeapMontage, Rate);
	AnimInstance->Montage_JumpToSection(StartSection, LeapMontage);

	UAbilityTask_WaitDelay* TakeoffTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(TakeoffTime / Rate, 0.01f));
	TakeoffTask->OnFinish.AddDynamic(this, &ThisClass::HandleTakeoff);
	TakeoffTask->ReadyForActivation();
}

void UAIGameplayAbility_BossJumpSlam::HandleTakeoff()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	BeginLeapMovement();

	const FVector OwnerFeet = OwnerCharacter->GetActorLocation() - FVector(0.f, 0.f, OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector ToLanding = GetLeapLandingLocation() - OwnerFeet;

	FVector LeapDirection = ToLanding.GetSafeNormal2D();

	if (LeapDirection.IsNearlyZero())
	{
		LeapDirection = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	// 포물선 이동. 시작 높이로 착지하며, 공중 섹션은 몽타주에서 반복
	UAbilityTask_ApplyRootMotionJumpForce* JumpTask = UAbilityTask_ApplyRootMotionJumpForce::ApplyRootMotionJumpForce(this, NAME_None,
		LeapDirection.Rotation(), ToLanding.Size2D(), LeapHeight, AirDuration, 0.f, false,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, nullptr, nullptr);
	JumpTask->OnFinish.AddDynamic(this, &ThisClass::HandleLanded);
	JumpTask->ReadyForActivation();
}

void UAIGameplayAbility_BossJumpSlam::HandleLanded()
{
	EndLeapMovement();

	if (UAnimInstance* AnimInstance = GetOwnerAnimInstance())
	{
		if (!AnimInstance->Montage_IsPlaying(LeapMontage))
		{
			AnimInstance->Montage_Play(LeapMontage, FMath::Max(PlayRate, 0.1f));
		}

		AnimInstance->Montage_JumpToSection(LandSection, LeapMontage);
	}

	UAbilityTask_WaitDelay* ImpactTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(LandImpactDelay / FMath::Max(PlayRate, 0.1f), 0.01f));
	ImpactTask->OnFinish.AddDynamic(this, &ThisClass::HandleLandImpact);
	ImpactTask->ReadyForActivation();
}

void UAIGameplayAbility_BossJumpSlam::HandleLandImpact()
{
	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	ApplyAreaDamage(DamageSpecHandle, true);

	if (LeapIndex + 1 < GetLeapCount())
	{
		UAbilityTask_WaitDelay* NextTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(NextLeapDelay, 0.01f));
		NextTask->OnFinish.AddDynamic(this, &ThisClass::HandleNextLeap);
		NextTask->ReadyForActivation();
		return;
	}

	// 마지막 착지: 착지 섹션이 끝날 때까지 기다린 뒤 종료
	float RemainingTime = 0.01f;

	if (LeapMontage)
	{
		const int32 LandSectionIndex = LeapMontage->GetSectionIndex(LandSection);

		if (LandSectionIndex != INDEX_NONE)
		{
			RemainingTime = FMath::Max((LeapMontage->GetSectionLength(LandSectionIndex) - LandImpactDelay) / FMath::Max(PlayRate, 0.1f), 0.01f);
		}
	}

	UAbilityTask_WaitDelay* FinishTask = UAbilityTask_WaitDelay::WaitDelay(this, RemainingTime);
	FinishTask->OnFinish.AddDynamic(this, &ThisClass::HandleFinished);
	FinishTask->ReadyForActivation();
}

void UAIGameplayAbility_BossJumpSlam::HandleNextLeap()
{
	++LeapIndex;
	StartLeap();
}

void UAIGameplayAbility_BossJumpSlam::HandleFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_BossJumpSlam::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bRadiusOverridden)
	{
		DefaultAreaData.Radius = SavedAreaRadius;
		bRadiusOverridden = false;
	}

	// 중간에 끊기면 공중/착지 몽타주를 멈춤
	if (bWasCancelled)
	{
		if (UAnimInstance* AnimInstance = GetOwnerAnimInstance(); AnimInstance && LeapMontage)
		{
			AnimInstance->Montage_Stop(0.2f, LeapMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimInstance* UAIGameplayAbility_BossJumpSlam::GetOwnerAnimInstance() const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	return OwnerCharacter && OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
}

int32 UAIGameplayAbility_BossJumpSlam::GetLeapCount() const
{
	return FMath::Max(LandingRadii.Num(), 1);
}
