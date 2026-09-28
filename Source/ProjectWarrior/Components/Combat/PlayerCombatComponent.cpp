// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCombatComponent.h"
#include "ProjectWarrior/Items/WeaponBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ProjectWarrior/WarriorGameplayTags.h"

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
