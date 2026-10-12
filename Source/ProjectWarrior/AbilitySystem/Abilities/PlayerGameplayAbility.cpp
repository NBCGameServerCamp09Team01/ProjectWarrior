// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/Controllers/WarriorPlayerController.h"
#include "ProjectWarrior/Components/Combat/PlayerCombatComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/Account/WarriorAccountTags.h"
#include "ProjectWarrior/Skill/WarriorSkillEffectSettings.h"
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

	InWeaponBaseDamage = ApplyCounterPlus(InWeaponBaseDamage);

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

	// 반격(GA_Player_Counter)은 이 균형 데미지로 때린다
	InWeaponBaseDamage = ApplyCounterPlus(InWeaponBaseDamage);

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

bool UPlayerGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 회피 숙련(S4): 설정이 켜져 있으면 회피는 해금 태그가 있어야 나간다(시연용. 끄면 예전처럼 늘 나감)
	if (GetDefault<UWarriorSkillEffectSettings>()->bDodgeRequiresDodgeMastery
		&& GetAssetTags().HasTagExact(WarriorGameplayTags::Player_Ability_Dodge))
	{
		const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		if (!ASC || !ASC->HasMatchingGameplayTag(WarriorAccountTags::Account_Skill_DodgeMastery))
		{
			return false;
		}
	}
	return true;
}

float UPlayerGameplayAbility::ApplyCounterPlus(float InBaseDamage)
{
	if (!GetAssetTags().HasTagExact(WarriorGameplayTags::Player_Ability_Counter))
	{
		return InBaseDamage;
	}
	const UWarriorAbilitySystemComponent* ASC = GetWarriorAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->HasMatchingGameplayTag(WarriorAccountTags::Account_Skill_CounterPlus))
	{
		return InBaseDamage;
	}
	return InBaseDamage * GetDefault<UWarriorSkillEffectSettings>()->CounterPlusDamageMultiplier;
}
