// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameplayAbility.h"
#include "AIGameplayAbility.generated.h"

class AWarriorAICharacter;
class UAICombatComponent;

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility : public UWarriorGameplayAbility
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    AWarriorAICharacter* GetAICharacterFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    UAICombatComponent* GetAICombatComponentFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    FGameplayEffectSpecHandle MakeAIDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, const FScalableFloat& InDamageScalableFloat);

private:
    TWeakObjectPtr<AWarriorAICharacter> CachedAICharacter;
};
