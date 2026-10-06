// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Characters/WarriorBossCharacter.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/Components/Combat/AICombatComponent.h"
#include "ProjectWarrior/Items/WeaponBase.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "ProjectWarrior/Components/Combat/AttackTokenComponent.h"

UAIGameplayAbility::UAIGameplayAbility()
{
    HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_Light;
}

bool UAIGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    // 토큰이 없으면 발동 실패 -> BT 태스크도 실패하므로 다른 행동(스트레이프 등)으로 넘어감
    if (UsesAttackToken(ActorInfo))
    {
        if (const UAttackTokenComponent* TokenComponent = FindTargetAttackTokenComponent(ActorInfo))
        {
            return TokenComponent->CanAcquire(ActorInfo->AvatarActor.Get(), AttackTokenPool, AttackTokenCost);
        }
    }

    return true;
}

void UAIGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    HeldAttackTokenComponent.Reset();

    if (UsesAttackToken(ActorInfo))
    {
        if (UAttackTokenComponent* TokenComponent = FindTargetAttackTokenComponent(ActorInfo))
        {
            // BT 데코레이터(UBTDecorator_AttackToken)가 받아 둔 토큰이 있으면 같이 붙잡음.
            // 데코레이터 분기가 공격보다 먼저 끝나도 공격이 끝날 때까지 토큰이 유지됨
            if (!TokenComponent->TryAcquire(ActorInfo->AvatarActor.Get(), AttackTokenPool, AttackTokenCost))
            {
                EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
                return;
            }

            HeldAttackTokenComponent = TokenComponent;
        }
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAIGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 정상 종료, 피격·사망으로 인한 취소 모두 여기서 반납
    if (UAttackTokenComponent* TokenComponent = HeldAttackTokenComponent.Get())
    {
        TokenComponent->Release(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    }
    HeldAttackTokenComponent.Reset();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAIGameplayAbility::UsesAttackToken(const FGameplayAbilityActorInfo* ActorInfo) const
{
    // 보스는 같은 어빌리티(AI.Ability.Melee 등)를 써도 토큰 제한 없이 공격
    const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    return AttackTokenPool.IsValid() && AvatarActor && !AvatarActor->IsA<AWarriorBossCharacter>();
}

UAttackTokenComponent* UAIGameplayAbility::FindTargetAttackTokenComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
    const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
    const AAIController* AIController = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;

    if (!AIController)
    {
        return nullptr;
    }

    AActor* TargetActor = nullptr;

    if (const UBlackboardComponent* BlackboardComponent = AIController->GetBlackboardComponent())
    {
        TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetActor")));
    }

    if (!TargetActor)
    {
        TargetActor = AIController->GetFocusActor();
    }

    return TargetActor ? TargetActor->FindComponentByClass<UAttackTokenComponent>() : nullptr;
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
