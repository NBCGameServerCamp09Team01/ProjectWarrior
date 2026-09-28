// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/Controllers/WarriorPlayerController.h"
#include "ProjectWarrior/Components/Combat/PlayerCombatComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

AWarriorPlayerCharacter* UPlayerGameplayAbility::GetPlayerCharacterFromActorInfo()
{
    if (!CachedWarriorPlayerCharacter.IsValid())
    {
        CachedWarriorPlayerCharacter = Cast<AWarriorPlayerCharacter>(CurrentActorInfo->AvatarActor);
    }

    return CachedWarriorPlayerCharacter.IsValid() ? CachedWarriorPlayerCharacter.Get() : nullptr;
}

AWarriorPlayerController* UPlayerGameplayAbility::GetPlayerControllerFromActorInfo()
{
    if (!CachedWarriorPlayerController.IsValid())
    {
        CachedWarriorPlayerController = Cast<AWarriorPlayerController>(CurrentActorInfo->PlayerController);
    }

    return CachedWarriorPlayerController.IsValid() ? CachedWarriorPlayerController.Get() : nullptr;
}

UPlayerCombatComponent* UPlayerGameplayAbility::GetPlayerCombatComponentFromActorInfo()
{
	return GetAvatarActorFromActorInfo()->FindComponentByClass<UPlayerCombatComponent>();
}

UWarriorAbilitySystemComponent* UPlayerGameplayAbility::GetPlayerAbilitySystemComponentFromActorInfo()
{
    return Cast<UWarriorAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
}

FGameplayEffectSpecHandle UPlayerGameplayAbility::MakePlayerDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag, int32 InUsedComboCount, FGameplayTag InHitReactTypeTag)
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
		InWeaponBaseDamage
	);

	if (InCurrentAttackTypeTag.IsValid())
	{
		EffectSpecHandle.Data->SetSetByCallerMagnitude(InCurrentAttackTypeTag, InUsedComboCount);
	}

	if (InHitReactTypeTag.IsValid())
	{
		EffectSpecHandle.Data->AddDynamicAssetTag(InHitReactTypeTag);
	}

	return EffectSpecHandle;
}

FGameplayEffectSpecHandle UPlayerGameplayAbility::MakePlayerBalanceDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage)
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
		InWeaponBaseDamage
	);

	return EffectSpecHandle;
}
