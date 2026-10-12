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

	// 해금해야 나가는 능력(S4 시연용): 설정이 켜져 있고 이 능력이 그 능력이면, 계정 해금 태그가 있어야 나간다.
	// 끄면 예전처럼 늘 나간다. GA .uasset은 고치지 않는다(능력 태그로 가린다)
	const UWarriorSkillEffectSettings* Settings = GetDefault<UWarriorSkillEffectSettings>();
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayTagContainer& OwnAssetTags = GetAssetTags();
	auto IsLocked = [&OwnAssetTags, ASC](bool bEnabled, const FGameplayTag& AbilityTag, const FGameplayTag& SkillTag)
	{
		return bEnabled && OwnAssetTags.HasTagExact(AbilityTag) && (!ASC || !ASC->HasMatchingGameplayTag(SkillTag));
	};

	// 회피 숙련: 회피(더블 탭 스텝)
	if (IsLocked(Settings->bDodgeRequiresDodgeMastery, WarriorGameplayTags::Player_Ability_Dodge, WarriorAccountTags::Account_Skill_DodgeMastery))
	{
		return false;
	}
	// 콤보 4타: 약공격 4타(GA_Katana_LightAttack_04). 해금 전에는 3타에서 콤보가 끝난다
	if (IsLocked(Settings->bCombo4RequiresUnlock, WarriorGameplayTags::Player_Ability_Attack_Katana_Light_4, WarriorAccountTags::Account_Skill_Combo4))
	{
		return false;
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
