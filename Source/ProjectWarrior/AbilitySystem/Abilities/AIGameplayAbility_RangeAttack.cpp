// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_RangeAttack.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

void UAIGameplayAbility_RangeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bDestroyHeldProjectileOnEnd)
	{
		DestroyHeldProjectile();
	}

	HeldProjectile.Reset();
	ClearLockedAimLocation();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AWarriorProjectileBase* UAIGameplayAbility_RangeAttack::SpawnHeldProjectile(TSubclassOf<AWarriorProjectileBase> ProjectileClass, FName AttachSocketName, bool bAttachToCharacterMesh)
{
	USceneComponent* SocketParent = GetProjectileSocketParent(bAttachToCharacterMesh);

	if (!SocketParent)
	{
		return nullptr;
	}

	DestroyHeldProjectile();

	AWarriorProjectileBase* Projectile = SpawnUnlaunchedProjectile(ProjectileClass, SocketParent->GetSocketTransform(AttachSocketName));

	if (Projectile)
	{
		Projectile->AttachToComponent(SocketParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
		HeldProjectile = Projectile;
	}

	return Projectile;
}

bool UAIGameplayAbility_RangeAttack::LaunchHeldProjectile(AActor* TargetActor, const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bPredictTargetMovement)
{
	AWarriorProjectileBase* Projectile = HeldProjectile.Get();

	if (!Projectile || Projectile->IsLaunched())
	{
		return false;
	}

	const FVector LaunchLocation = Projectile->GetActorLocation();

	bool bTargetDodged = false;

	if (bHasLockedAimLocation)
	{
		// 발사 순간 회피 중인 경우도 회피로 간주
		const bool bDodgingNow = LockedTargetASC.IsValid() && LockedTargetASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Dodge);

		bTargetDodged = bLockedTargetDodged || bDodgingNow;
	}

	const bool bUseLockedAim = bHasLockedAimLocation && (!bOnlyUseLockedAimWhenDodged || bTargetDodged);

	// 고정 대상이 있으면 그 대상을 추적 (TargetActor보다 우선)
	AActor* AimTarget = LockedTarget.IsValid() ? LockedTarget.Get() : TargetActor;

	FVector LaunchDirection;

	if (bUseLockedAim)
	{
		const FVector AimLocation = (bTargetDodged && LockedTarget.IsValid())
			? ComputeDodgeCorrectedAimLocation(LaunchLocation, LockedTarget->GetActorLocation())
			: LockedAimLocation;

		LaunchDirection = (AimLocation - LaunchLocation).GetSafeNormal();
	}
	else
	{
		LaunchDirection = ComputeProjectileLaunchDirection(LaunchLocation, AimTarget, Projectile->GetProjectileMovement()->InitialSpeed, bPredictTargetMovement);
	}

	Projectile->LaunchProjectile(LaunchDirection, InDamageSpecHandle);
	HeldProjectile.Reset();

	return true;
}

bool UAIGameplayAbility_RangeAttack::LockAimLocation(AActor* TargetActor)
{
	ClearLockedAimLocation();

	AActor* ResolvedTarget = ResolveProjectileTarget(TargetActor);

	if (!ResolvedTarget)
	{
		return false;
	}

	LockedAimLocation = ResolvedTarget->GetActorLocation();
	bHasLockedAimLocation = true;
	LockedTarget = ResolvedTarget;

	// 이미 회피 중이던 것은 제외하고, 이후 새로 시작한 회피만 감지
	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ResolvedTarget))
	{
		LockedTargetASC = TargetASC;
		DodgeTagDelegateHandle = TargetASC->RegisterGameplayTagEvent(WarriorGameplayTags::Shared_Status_Dodge, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnLockedTargetDodgeTagChanged);
	}

	return true;
}

void UAIGameplayAbility_RangeAttack::ClearLockedAimLocation()
{
	StopWatchingLockedTarget();

	bHasLockedAimLocation = false;
	bLockedTargetDodged = false;
	LockedAimLocation = FVector::ZeroVector;
	LockedTarget.Reset();
}

FVector UAIGameplayAbility_RangeAttack::ComputeDodgeCorrectedAimLocation(const FVector& LaunchLocation, const FVector& TargetLocation) const
{
	const FVector ToTargetDirection = (TargetLocation - LaunchLocation).GetSafeNormal();

	if (ToTargetDirection.IsNearlyZero())
	{
		return LockedAimLocation;
	}

	// 대상 기준 고정 조준점의 오프셋 중 궤적에 수직인 성분 = 고정 위치로 쐈을 때 대상에게서 빗나가는 거리
	const FVector LockedOffset = LockedAimLocation - TargetLocation;
	const FVector LateralOffset = LockedOffset - ToTargetDirection * FVector::DotProduct(LockedOffset, ToTargetDirection);

	FVector LateralDirection = LateralOffset.GetSafeNormal();

	// 궤적 방향으로만 회피한 경우(앞/뒤) 고정 위치로 쏘면 그대로 맞으므로 옆으로 비껴 쏨
	if (LateralDirection.IsNearlyZero())
	{
		LateralDirection = FVector::CrossProduct(ToTargetDirection, FVector::UpVector).GetSafeNormal();

		if (LateralDirection.IsNearlyZero())
		{
			LateralDirection = FVector::RightVector;
		}
	}

	const float CorrectedMissDistance = FMath::Max(LateralOffset.Size() * (1.f - DodgeAimCorrectionRatio), MinDodgeMissDistance);

	return TargetLocation + LateralDirection * CorrectedMissDistance;
}

void UAIGameplayAbility_RangeAttack::OnLockedTargetDodgeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		bLockedTargetDodged = true;
	}
}

void UAIGameplayAbility_RangeAttack::StopWatchingLockedTarget()
{
	if (UAbilitySystemComponent* TargetASC = LockedTargetASC.Get())
	{
		TargetASC->RegisterGameplayTagEvent(WarriorGameplayTags::Shared_Status_Dodge, EGameplayTagEventType::NewOrRemoved)
			.Remove(DodgeTagDelegateHandle);
	}

	DodgeTagDelegateHandle.Reset();
	LockedTargetASC.Reset();
}

AWarriorProjectileBase* UAIGameplayAbility_RangeAttack::GetHeldProjectile() const
{
	return HeldProjectile.Get();
}

void UAIGameplayAbility_RangeAttack::DestroyHeldProjectile()
{
	if (AWarriorProjectileBase* Projectile = HeldProjectile.Get())
	{
		if (!Projectile->IsLaunched())
		{
			Projectile->Destroy();
		}
	}

	HeldProjectile.Reset();
}
