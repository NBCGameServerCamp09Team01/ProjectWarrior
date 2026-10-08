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
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
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

    if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
        const FGameplayTagContainer& OwnAssetTags = GetAssetTags();

        // 슈퍼아머 중에는 피격 경직이 발동하지 않음 (가드 반격 등이 끊기지 않게)
        if (OwnAssetTags.HasTag(WarriorGameplayTags::Shared_Ability_HitReact) && ASC->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_SuperArmor))
        {
            return false;
        }

        // 회피·가드 중에는 공격하지 않음
        static const FGameplayTagContainer AttackAbilityTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ WarriorGameplayTags::AI_Ability_Melee, WarriorGameplayTags::AI_Ability_Range, WarriorGameplayTags::AI_Ability_Special });
        static const FGameplayTagContainer DefensiveStatusTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ WarriorGameplayTags::AI_Status_Dodging, WarriorGameplayTags::AI_Status_Guarding });
        if (OwnAssetTags.HasAny(AttackAbilityTags) && ASC->HasAnyMatchingGameplayTags(DefensiveStatusTags))
        {
            return false;
        }
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

    const bool bWasActive = IsActive();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

    // 이 어빌리티의 활성 태그(회피 중 등)가 빠진 뒤에 후속 어빌리티 발동
    if (bWasActive && !bWasCancelled && FollowUpAbilityTag.IsValid() && FMath::FRand() < FollowUpChance
        && IsTargetInFollowUpRange(ActorInfo))
    {
        if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
        {
            ASC->TryActivateAbilitiesByTag(FollowUpAbilityTag.GetSingleTagContainer());
        }
    }
}

void UAIGameplayAbility::WatchForFinisherOrDeath()
{
    // 처형은 이벤트(AI.Event.Finisher)가 태그(Shared.Status.Finisher)보다 먼저 오므로 둘 다 감시
    UAbilityTask_WaitGameplayEvent* FinisherEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WarriorGameplayTags::AI_Event_Finisher, nullptr, true, true);
    FinisherEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleFinisherOrDeathEvent);
    FinisherEventTask->ReadyForActivation();

    // FNativeGameplayTag는 복사할 수 없으므로 FGameplayTag 배열로 변환해 순회
    const FGameplayTag WatchedStatusTags[] = { WarriorGameplayTags::Shared_Status_Finisher, WarriorGameplayTags::Shared_Status_Death };
    for (const FGameplayTag& Tag : WatchedStatusTags)
    {
        UAbilityTask_WaitGameplayTagAdded* TagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tag, nullptr, true);
        TagTask->Added.AddDynamic(this, &ThisClass::HandleFinisherOrDeathTag);
        TagTask->ReadyForActivation();
    }
}

bool UAIGameplayAbility::IsOwnerIncapacitated(const UAbilitySystemComponent* ASC)
{
    if (!ASC)
    {
        return false;
    }

    static const FGameplayTagContainer IncapacitatedStatusTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
        WarriorGameplayTags::Shared_Status_Finisher, WarriorGameplayTags::Shared_Status_Death });
    if (ASC->HasAnyMatchingGameplayTags(IncapacitatedStatusTags))
    {
        return true;
    }

    static const FGameplayTagContainer IncapacitatedAbilityTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
        WarriorGameplayTags::Shared_Ability_HitReact, WarriorGameplayTags::Shared_Ability_Stagger, WarriorGameplayTags::AI_Ability_Finisher });
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.IsActive() && Spec.Ability && Spec.Ability->GetAssetTags().HasAny(IncapacitatedAbilityTags))
        {
            return true;
        }
    }

    return false;
}

void UAIGameplayAbility::HandleFinisherOrDeathEvent(FGameplayEventData Payload)
{
    CancelForFinisherOrDeath();
}

void UAIGameplayAbility::HandleFinisherOrDeathTag()
{
    CancelForFinisherOrDeath();
}

void UAIGameplayAbility::CancelForFinisherOrDeath()
{
    // 처형·사망 몽타주는 그쪽 어빌리티가 재생하므로 이 어빌리티만 취소
    if (IsActive())
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
    }
}

bool UAIGameplayAbility::UsesAttackToken(const FGameplayAbilityActorInfo* ActorInfo) const
{
    // 보스는 같은 어빌리티(AI.Ability.Melee 등)를 써도 토큰 제한 없이 공격
    const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    return AttackTokenPool.IsValid() && AvatarActor && !AvatarActor->IsA<AWarriorBossCharacter>();
}

bool UAIGameplayAbility::IsTargetInFollowUpRange(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (FollowUpMaxTargetDistance <= 0.f)
    {
        return true;
    }

    const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    const AActor* TargetActor = FindAITargetActor(ActorInfo);

    if (!Avatar || !TargetActor)
    {
        return false;
    }

    return FVector::DistSquared2D(Avatar->GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(FollowUpMaxTargetDistance);
}

AActor* UAIGameplayAbility::FindAITargetActor(const FGameplayAbilityActorInfo* ActorInfo)
{
    const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
    const AAIController* AIController = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;

    if (!AIController)
    {
        return nullptr;
    }

    if (const UBlackboardComponent* BlackboardComponent = AIController->GetBlackboardComponent())
    {
        if (AActor* TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetActor"))))
        {
            return TargetActor;
        }
    }

    return AIController->GetFocusActor();
}

UAttackTokenComponent* UAIGameplayAbility::FindTargetAttackTokenComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
    const AActor* TargetActor = FindAITargetActor(ActorInfo);
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
