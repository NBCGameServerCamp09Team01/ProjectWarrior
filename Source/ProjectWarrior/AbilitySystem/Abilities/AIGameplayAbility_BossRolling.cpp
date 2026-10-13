// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossRolling.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "Particles/ParticleSystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

UAIGameplayAbility_BossRolling::UAIGameplayAbility_BossRolling()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	StunnedStatusTag = WarriorGameplayTags::AI_Status_Boss_Stunned;
}

void UAIGameplayAbility_BossRolling::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 첫 방향은 대상 쪽
	FaceAreaTarget();
	RollDirection = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();

	if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	if (UAnimInstance* AnimInstance = GetOwnerAnimInstance(); AnimInstance && RollMontage)
	{
		AnimInstance->Montage_Play(RollMontage);
		AnimInstance->Montage_JumpToSection(StartSection, RollMontage);
	}

	UAbilityTask_WaitDelay* StartTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(RollStartDelay, 0.01f));
	StartTask->OnFinish.AddDynamic(this, &ThisClass::HandleRollStart);
	StartTask->ReadyForActivation();
}

void UAIGameplayAbility_BossRolling::HandleRollStart()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();

	if (!OwnerCharacter || !World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (UAnimInstance* AnimInstance = GetOwnerAnimInstance(); AnimInstance && RollMontage)
	{
		if (!AnimInstance->Montage_IsPlaying(RollMontage))
		{
			AnimInstance->Montage_Play(RollMontage);
		}

		AnimInstance->Montage_JumpToSection(LoopSection, RollMontage);
	}

	// 굴러가는 동안 대상을 밀어내지 않고 지나가며 피해를 줌
	if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent(); Capsule && !bAppliedCollisionIgnore)
	{
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bAppliedCollisionIgnore = true;
	}

	RollDamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	RollProcessedActors.Reset();
	RollHitTimes.Reset();

	bRolling = true;
	RollEndTime = World->GetTimeSeconds() + RollDuration;

	StartRollMovement();

	StopRollTrail();
	RollTrailComponent = UWarriorFunctionLibrary::SpawnFXAttached(RollTrailEffect, OwnerCharacter->GetRootComponent(), NAME_None, RollTrailEffectScale, true);

	// 캡슐 바닥(발밑)에 붙임. 캡슐 기준 상대 위치라 보스 크기가 바뀌어도 항상 발밑
	if (RollTrailComponent)
	{
		if (const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
		{
			RollTrailComponent->SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
		}
	}

	World->GetTimerManager().SetTimer(RollTickHandle, this, &ThisClass::TickRolling, 1.f / 60.f, true);

	UAbilityTask_WaitDelay* RollTask = UAbilityTask_WaitDelay::WaitDelay(this, RollDuration);
	RollTask->OnFinish.AddDynamic(this, &ThisClass::HandleRollFinished);
	RollTask->ReadyForActivation();
}

void UAIGameplayAbility_BossRolling::TickRolling()
{
	if (!bRolling)
	{
		return;
	}

	if (TryBounce())
	{
		StartRollMovement();
	}

	ApplyRollDamage();
}

void UAIGameplayAbility_BossRolling::StartRollMovement()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UWorld* World = GetWorld();

	if (!OwnerCharacter || !World)
	{
		return;
	}

	StopRollMovement();

	SetOwnerFacingRotation(RollDirection.Rotation());

	const float RemainingTime = FMath::Max(RollEndTime - World->GetTimeSeconds(), 0.05f);

	// 수평 속도를 덮어써서 일정한 속도로 굴러감 (중력은 유지해 바닥을 따라감)
	RollMoveTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, NAME_None, RollDirection, RollSpeed, RemainingTime,
		false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
	RollMoveTask->ReadyForActivation();
}

void UAIGameplayAbility_BossRolling::StopRollMovement()
{
	if (RollMoveTask)
	{
		RollMoveTask->EndTask();
		RollMoveTask = nullptr;
	}
}

bool UAIGameplayAbility_BossRolling::TryBounce()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();

	if (!OwnerCharacter || !World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// 바닥 턱에 걸리지 않도록 캡슐을 조금 띄워서 앞을 검사
	const FVector Start = OwnerCharacter->GetActorLocation() + FVector(0.f, 0.f, 10.f);
	const FVector End = Start + RollDirection * WallProbeDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossRollingWallProbe), false, OwnerCharacter);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;

	if (!World->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius * 0.9f, FMath::Max(HalfHeight - 10.f, Radius * 0.9f)), QueryParams))
	{
		return false;
	}

	// 바닥·완만한 경사는 무시하고 벽만 튕김
	const float MaxNormalZ = FMath::Sin(FMath::DegreesToRadians(WallMaxSlopeFromVertical));

	if (FMath::Abs(Hit.ImpactNormal.Z) > MaxNormalZ)
	{
		return false;
	}

	const FVector WallNormal = Hit.ImpactNormal.GetSafeNormal2D();

	// 벽 쪽으로 가고 있을 때만 반사 (이미 벽에서 멀어지는 중이면 그대로)
	if (WallNormal.IsNearlyZero() || FVector::DotProduct(RollDirection, WallNormal) >= 0.f)
	{
		return false;
	}

	FVector Reflected = RollDirection - 2.f * FVector::DotProduct(RollDirection, WallNormal) * WallNormal;
	Reflected = Reflected.RotateAngleAxis(FMath::FRandRange(-BounceRandomAngle, BounceRandomAngle), FVector::UpVector).GetSafeNormal2D();

	// 무작위 각도 때문에 다시 벽 쪽을 향하면 반사 방향 그대로
	if (FVector::DotProduct(Reflected, WallNormal) <= 0.1f)
	{
		Reflected = (RollDirection - 2.f * FVector::DotProduct(RollDirection, WallNormal) * WallNormal).GetSafeNormal2D();
	}

	RollDirection = Reflected;

	if (BounceEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BounceEffect, Hit.ImpactPoint, WallNormal.Rotation());
	}

	return true;
}

void UAIGameplayAbility_BossRolling::ApplyRollDamage()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UWorld* World = GetWorld();

	if (!OwnerCharacter || !World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// HitInterval이 지난 대상은 다시 맞을 수 있게 목록에서 뺌
	for (auto It = RollHitTimes.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || Now - It->Value >= HitInterval)
		{
			RollProcessedActors.Remove(It->Key);
			It.RemoveCurrent();
		}
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();

	FWarriorAttackAreaData RollArea = DefaultAreaData;
	RollArea.Shape = EWarriorAttackAreaShape::Circle;
	RollArea.Radius = Capsule->GetScaledCapsuleRadius() + HitRadiusPadding;
	RollArea.InnerRadius = 0.f;
	RollArea.Height = Capsule->GetScaledCapsuleHalfHeight() * 2.f;

	const FTransform AreaTransform(RollDirection.Rotation(), OwnerCharacter->GetActorLocation());

	ApplyAreaDamageAt(RollArea, AreaTransform, RollDamageSpecHandle, &RollProcessedActors);

	// 이번에 새로 판정한 대상의 시각 기록
	for (const TWeakObjectPtr<AActor>& ProcessedActor : RollProcessedActors)
	{
		if (!RollHitTimes.Contains(ProcessedActor))
		{
			RollHitTimes.Add(ProcessedActor, Now);
		}
	}
}

void UAIGameplayAbility_BossRolling::HandleRollFinished()
{
	bRolling = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RollTickHandle);
	}

	StopRollMovement();
	StopRollTrail();
	RestorePawnCollision();

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}

	SetStunned(true);

	if (UAnimInstance* AnimInstance = GetOwnerAnimInstance())
	{
		if (StunMontage)
		{
			AnimInstance->Montage_Play(StunMontage);
		}
		else if (RollMontage)
		{
			AnimInstance->Montage_Stop(0.25f, RollMontage);
		}
	}

	UAbilityTask_WaitDelay* StunTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(StunDuration, 0.01f));
	StunTask->OnFinish.AddDynamic(this, &ThisClass::HandleStunFinished);
	StunTask->ReadyForActivation();
}

void UAIGameplayAbility_BossRolling::HandleStunFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_BossRolling::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bRolling = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RollTickHandle);
	}

	StopRollMovement();
	StopRollTrail();
	RestorePawnCollision();
	SetStunned(false);

	if (UAnimInstance* AnimInstance = GetOwnerAnimInstance())
	{
		for (UAnimMontage* Montage : { RollMontage.Get(), StunMontage.Get() })
		{
			if (Montage && AnimInstance->Montage_IsPlaying(Montage))
			{
				AnimInstance->Montage_Stop(0.25f, Montage);
			}
		}
	}

	RollProcessedActors.Reset();
	RollHitTimes.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_BossRolling::SetStunned(bool bStunned)
{
	if (bStunned == bAppliedStunTag || !StunnedStatusTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (bStunned)
		{
			ASC->AddLooseGameplayTag(StunnedStatusTag);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(StunnedStatusTag);
		}

		bAppliedStunTag = bStunned;
	}
}

void UAIGameplayAbility_BossRolling::StopRollTrail()
{
	// 새 입자만 멈추고 남은 흙먼지는 자연스럽게 사라지도록 Deactivate (자동 파괴)
	if (RollTrailComponent)
	{
		RollTrailComponent->Deactivate();
		RollTrailComponent = nullptr;
	}
}

void UAIGameplayAbility_BossRolling::RestorePawnCollision()
{
	if (!bAppliedCollisionIgnore)
	{
		return;
	}

	bAppliedCollisionIgnore = false;

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (UCapsuleComponent* Capsule = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr)
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
	}
}

UAnimInstance* UAIGameplayAbility_BossRolling::GetOwnerAnimInstance() const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	return OwnerCharacter && OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
}
