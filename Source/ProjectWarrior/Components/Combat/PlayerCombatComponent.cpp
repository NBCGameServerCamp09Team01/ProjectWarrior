// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCombatComponent.h"
#include "ProjectWarrior/Items/WeaponBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ProjectWarrior/WarriorGameplayTags.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "EngineUtils.h"

void UPlayerCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
	}
}

void UPlayerCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UPlayerCombatComponent::HandleAbilityActivated(UGameplayAbility* ActivatedAbility)
{
	// 처형은 회피 대상이 아니므로 알리지 않음
	if (ActivatedAbility && ActivatedAbility->GetAssetTags().HasTag(WarriorGameplayTags::Player_Ability_Attack)
		&& !ActivatedAbility->GetAssetTags().HasTag(WarriorGameplayTags::Player_Ability_Finisher))
	{
		NotifyIncomingAttack();
	}
}

void UPlayerCombatComponent::NotifyIncomingAttack() const
{
	APawn* OwningPawn = GetOwningPawn();
	UWorld* World = GetWorld();
	if (!OwningPawn || !World)
	{
		return;
	}

	const FVector Origin = OwningPawn->GetActorLocation();
	const FVector Forward = OwningPawn->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(IncomingAttackHalfAngle));

	for (TActorIterator<AWarriorAICharacter> It(World); It; ++It)
	{
		AWarriorAICharacter* Enemy = *It;
		const FVector ToEnemy = Enemy->GetActorLocation() - Origin;

		if (ToEnemy.SizeSquared2D() > FMath::Square(IncomingAttackRadius)
			|| FVector::DotProduct(Forward, ToEnemy.GetSafeNormal2D()) < MinDot
			|| UWarriorFunctionLibrary::IsActorDead(Enemy))
		{
			continue;
		}

		FGameplayEventData Data;
		Data.Instigator = OwningPawn;
		Data.Target = Enemy;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Enemy, WarriorGameplayTags::AI_Event_IncomingAttack, Data);
	}
}

AWeaponBase* UPlayerCombatComponent::GetCarriedWeaponByTag(FGameplayTag InWeaponTag) const
{
    return Cast<AWeaponBase>(GetCharacterCarriedWeaponByTag(InWeaponTag));
}

AWeaponBase* UPlayerCombatComponent::GetCurrentEquippedWeapon() const
{
	return Cast<AWeaponBase>(GetCharacterCurrentEquippedWeapon());
}

float UPlayerCombatComponent::GetPlayerCurrentEquippedWeaponDamageAtLevel(float InLevel) const
{
	return GetCurrentEquippedWeapon()->PlayerWeaponData.WeaponBaseDamage.GetValueAtLevel(InLevel);
}

void UPlayerCombatComponent::OnHitTargetActor(AActor* HitActor)
{
	if (OverlappedActors.Contains(HitActor))
	{
		return;
	}

	OverlappedActors.AddUnique(HitActor);

	// 회피 중인 대상은 피해·히트스톱 없음
	if (UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitActor, WarriorGameplayTags::Shared_Status_Dodge))
	{
		return;
	}

	// 정면에서 가드 중이면 막힘. 대상에게 가드 피격을, 플레이어에게 막힘을 알림 (뒤나 옆에서 치면 그대로 맞음)
	if (UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitActor, WarriorGameplayTags::AI_Status_Guarding)
		&& UWarriorFunctionLibrary::IsValidBlock(GetOwningPawn(), HitActor))
	{
		FGameplayEventData BlockData;
		BlockData.Instigator = GetOwningPawn();
		BlockData.Target = HitActor;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitActor, WarriorGameplayTags::AI_Event_GuardHit, BlockData);
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwningPawn(), WarriorGameplayTags::Player_Event_AttackBlocked, BlockData);
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwningPawn(), WarriorGameplayTags::Player_Event_HitStop, FGameplayEventData());
		return;
	}

	FGameplayEventData Data;
	Data.Instigator = GetOwningPawn();
	Data.Target = HitActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		WarriorGameplayTags::Shared_Event_MeleeHit,
		Data
	);

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		WarriorGameplayTags::Player_Event_HitStop,
		FGameplayEventData()
	);
}

void UPlayerCombatComponent::OnWeaponPulledFromTargetActor(AActor* InteractedActor)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		WarriorGameplayTags::Player_Event_HitStop,
		FGameplayEventData()
	);
}
