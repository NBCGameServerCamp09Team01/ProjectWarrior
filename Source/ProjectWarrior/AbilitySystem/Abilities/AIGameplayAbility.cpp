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

UAIGameplayAbility::UAIGameplayAbility()
{
    HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_Light;
}

FGameplayTag UAIGameplayAbility::ResolveHitReactEventTag(const FGameplayEventData& InPayload) const
{
    for (const FGameplayTag& PayloadTag : InPayload.InstigatorTags)
    {
        if (PayloadTag.MatchesTag(WarriorGameplayTags::Shared_Event_HitReact))
        {
            return PayloadTag;
        }
    }

    return HitReactEventTag.IsValid() ? HitReactEventTag : WarriorGameplayTags::Shared_Event_HitReact_Light;
}

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
	USceneComponent* SocketParent = GetProjectileSocketParent(false);

	if (!SocketParent)
	{
		return nullptr;
	}

	const FVector SpawnLocation = SocketParent->DoesSocketExist(SpawnSocketName)
		? SocketParent->GetSocketLocation(SpawnSocketName)
		: GetAICharacterFromActorInfo()->GetActorLocation() + GetAICharacterFromActorInfo()->GetActorForwardVector() * 100.f;

	AWarriorProjectileBase* Projectile = SpawnUnlaunchedProjectile(ProjectileClass, FTransform(SpawnLocation));

	if (Projectile)
	{
		const FVector LaunchDirection = ComputeProjectileLaunchDirection(SpawnLocation, TargetActor, Projectile->GetProjectileMovement()->InitialSpeed, bPredictTargetMovement);
		Projectile->LaunchProjectile(LaunchDirection, InDamageSpecHandle);
	}

	return Projectile;
}

USceneComponent* UAIGameplayAbility::GetProjectileSocketParent(bool bUseCharacterMesh)
{
	AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo();

	if (!AICharacter)
	{
		return nullptr;
	}

	if (!bUseCharacterMesh)
	{
		if (AWeaponBase* EquippedWeapon = GetAICombatComponentFromActorInfo()->GetCharacterCurrentEquippedWeapon())
		{
			if (UMeshComponent* WeaponMesh = EquippedWeapon->GetWeaponMesh())
			{
				return WeaponMesh;
			}
		}
	}

	return AICharacter->GetMesh();
}

AWarriorProjectileBase* UAIGameplayAbility::SpawnUnlaunchedProjectile(TSubclassOf<AWarriorProjectileBase> ProjectileClass, const FTransform& SpawnTransform)
{
	check(ProjectileClass);

	AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo();

	if (!AICharacter || !AICharacter->HasAuthority())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AICharacter;
	SpawnParams.Instigator = AICharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return GetWorld()->SpawnActor<AWarriorProjectileBase>(ProjectileClass, SpawnTransform, SpawnParams);
}

AActor* UAIGameplayAbility::ResolveProjectileTarget(AActor* TargetActor)
{
	if (TargetActor)
	{
		return TargetActor;
	}

	if (AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo())
	{
		if (AAIController* AIController = Cast<AAIController>(AICharacter->GetController()))
		{
			return AIController->GetFocusActor();
		}
	}

	return nullptr;
}

FVector UAIGameplayAbility::ComputeProjectileLaunchDirection(const FVector& LaunchLocation, AActor* TargetActor, float ProjectileSpeed, bool bPredictTargetMovement)
{
	AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo();

	TargetActor = ResolveProjectileTarget(TargetActor);

	if (!TargetActor)
	{
		return AICharacter ? AICharacter->GetActorForwardVector() : FVector::ForwardVector;
	}

	FVector AimLocation = TargetActor->GetActorLocation();

	if (bPredictTargetMovement && ProjectileSpeed > 0.f)
	{
		const float TravelTime = FVector::Dist(LaunchLocation, AimLocation) / ProjectileSpeed;
		AimLocation += TargetActor->GetVelocity() * TravelTime;
	}

	return (AimLocation - LaunchLocation).GetSafeNormal();
}
