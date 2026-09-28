// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAsset_AIStartUpData.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/Abilities/AIGameplayAbility.h"

void UDataAsset_AIStartUpData::GiveToAbilitySystemComponent(UWarriorAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
    Super::GiveToAbilitySystemComponent(InASCToGive, ApplyLevel);

    if (!AICombatAbilities.IsEmpty())
    {
        for (const TSubclassOf<UAIGameplayAbility>& AbilityClass : AICombatAbilities)
        {
            if (!AbilityClass) continue;

            FGameplayAbilitySpec AbilitySpec(AbilityClass);
            AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
            AbilitySpec.Level = ApplyLevel;

            InASCToGive->GiveAbility(AbilitySpec);
        }
    }
}
