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

private:
    TWeakObjectPtr<AWarriorPlayerCharacter> CachedWarriorPlayerCharacter;
    TWeakObjectPtr<AWarriorPlayerController> CachedWarriorPlayerController;
};
