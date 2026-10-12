// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameplayAbility.h"
#include "PlayerGameplayAbility.generated.h"

class AWarriorPlayerCharacter;
class AWarriorPlayerController;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UPlayerGameplayAbility : public UWarriorGameplayAbility
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    AWarriorPlayerCharacter* GetPlayerCharacterFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    AWarriorPlayerController* GetPlayerControllerFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    UPlayerCombatComponent* GetPlayerCombatComponentFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    UWarriorAbilitySystemComponent* GetPlayerAbilitySystemComponentFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    FGameplayEffectSpecHandle MakePlayerDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag, int32 InUsedComboCount, FGameplayTag InHitReactTypeTag);

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    FGameplayEffectSpecHandle MakePlayerBalanceDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage);

    // 회피 숙련(S4): 회피(Player.Ability.Dodge)는 계정에서 스킬을 해금해야(Account.Skill.DodgeMastery 태그) 쓸 수 있다
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

private:
    // 반격 강화(S4): 반격 능력이고 스킬을 해금했으면(Account.Skill.CounterPlus 태그) 기본 데미지에 배율을 곱한다
    float ApplyCounterPlus(float InBaseDamage);

    TWeakObjectPtr<AWarriorPlayerCharacter> CachedWarriorPlayerCharacter;
    TWeakObjectPtr<AWarriorPlayerController> CachedWarriorPlayerController;
};
