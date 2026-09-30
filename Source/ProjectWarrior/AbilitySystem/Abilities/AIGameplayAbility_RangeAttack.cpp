// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_RangeAttack.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"

void UAIGameplayAbility_RangeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bDestroyHeldProjectileOnEnd)
	{
		DestroyHeldProjectile();
	}

	HeldProjectile.Reset();

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

	const FVector LaunchDirection = ComputeProjectileLaunchDirection(
		Projectile->GetActorLocation(),
		TargetActor,
		Projectile->GetProjectileMovement()->InitialSpeed,
		bPredictTargetMovement
	);

	Projectile->LaunchProjectile(LaunchDirection, InDamageSpecHandle);
	HeldProjectile.Reset();

	return true;
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
