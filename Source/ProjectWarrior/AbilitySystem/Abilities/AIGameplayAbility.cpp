// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/Components/Combat/AICombatComponent.h"
#include "ProjectWarrior/Items/WeaponBase.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AIController.h"

AWarriorAICharacter* UAIGameplayAbility::GetAICharacterFromActorInfo()
{
    if (!CachedAICharacter.IsValid())
    {
        CachedAICharacter = Cast<AWarriorAICharacter>(CurrentActorInfo->AvatarActor);
    }

    return CachedAICharacter.IsValid() ? CachedAICharacter.Get() : nullptr;
}

UAICombatComponent* UAIGameplayAbility::GetAICombatComponentFromActorInfo()
{
    return GetAICharacterFromActorInfo()->GetAICombatComponent();
}

FGameplayEffectSpecHandle UAIGameplayAbility::MakeAIDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, const FScalableFloat& InDamageScalableFloat)
{
	check(EffectClass);

	FGameplayEffectContextHandle ContextHandle = GetWarriorAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	ContextHandle.SetAbility(this);
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());
	ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle EffectSpecHandle = GetWarriorAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
		EffectClass,
		GetAbilityLevel(),
		ContextHandle
	);

	EffectSpecHandle.Data->SetSetByCallerMagnitude(
		WarriorGameplayTags::Shared_SetByCaller_BaseDamage,
		InDamageScalableFloat.GetValueAtLevel(GetAbilityLevel())
	);

	return EffectSpecHandle;
}

AWarriorProjectileBase* UAIGameplayAbility::SpawnProjectileFromEquippedWeapon(TSubclassOf<AWarriorProjectileBase> ProjectileClass, FName SpawnSocketName, AActor* TargetActor, const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bPredictTargetMovement)
{
	check(ProjectileClass);

	AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo();

	if (!AICharacter || !AICharacter->HasAuthority())
	{
		return nullptr;
	}

	FVector SpawnLocation = AICharacter->GetActorLocation() + AICharacter->GetActorForwardVector() * 100.f;

	if (AWeaponBase* EquippedWeapon = GetAICombatComponentFromActorInfo()->GetCharacterCurrentEquippedWeapon())
	{
		UMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh();

		if (WeaponMesh && WeaponMesh->DoesSocketExist(SpawnSocketName))
		{
			SpawnLocation = WeaponMesh->GetSocketLocation(SpawnSocketName);
		}
	}

	if (!TargetActor)
	{
		if (AAIController* AIController = Cast<AAIController>(AICharacter->GetController()))
		{
			TargetActor = AIController->GetFocusActor();
		}
	}

	FVector AimLocation = SpawnLocation + AICharacter->GetActorForwardVector() * 1000.f;

	if (TargetActor)
	{
		AimLocation = TargetActor->GetActorLocation();

		const float ProjectileSpeed = ProjectileClass->GetDefaultObject<AWarriorProjectileBase>()->GetProjectileMovement()->InitialSpeed;

		if (bPredictTargetMovement && ProjectileSpeed > 0.f)
		{
			const float TravelTime = FVector::Dist(SpawnLocation, AimLocation) / ProjectileSpeed;
			AimLocation += TargetActor->GetVelocity() * TravelTime;
		}
	}

	const FTransform SpawnTransform((AimLocation - SpawnLocation).Rotation(), SpawnLocation);

	AWarriorProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AWarriorProjectileBase>(
		ProjectileClass,
		SpawnTransform,
		AICharacter,
		AICharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	if (Projectile)
	{
		Projectile->ProjectileDamageEffectSpecHandle = InDamageSpecHandle;
		Projectile->FinishSpawning(SpawnTransform);
	}

	return Projectile;
}
