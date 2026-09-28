// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAsset_PlayerStartUpData.h"
#include "ProjectWarrior/AbilitySystem/Abilities/WarriorGameplayAbility.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/Types/WarriorStructTypes.h"

void UDataAsset_PlayerStartUpData::GiveToAbilitySystemComponent(UWarriorAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
    Super::GiveToAbilitySystemComponent(InASCToGive, ApplyLevel);

    for (const FWarriorPlayerAbilitySet& AbilitySet : HeroStartUpAbilitySets)
    {
        if (!AbilitySet.IsValid()) continue;

        FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
        AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
        AbilitySpec.Level = ApplyLevel;
        AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);

        InASCToGive->GiveAbility(AbilitySpec);
    }
}
