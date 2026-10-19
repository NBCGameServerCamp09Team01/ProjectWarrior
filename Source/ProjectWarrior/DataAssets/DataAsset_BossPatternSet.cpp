// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAsset_BossPatternSet.h"

bool FWarriorBossPatternData::IsUsableInPhase(int32 InPhase) const
{
	return InPhase >= MinPhase && (MaxPhase <= 0 || InPhase <= MaxPhase);
}

const FWarriorBossPatternData* UDataAsset_BossPatternSet::FindPattern(const FGameplayTag& InAbilityTag) const
{
	return Patterns.FindByPredicate([&InAbilityTag](const FWarriorBossPatternData& Pattern)
		{
			return Pattern.AbilityTag == InAbilityTag;
		});
}

const FWarriorBossPatternData* UDataAsset_BossPatternSet::FindPattern(const FGameplayTag& InAbilityTag, int32 InPhase) const
{
	return Patterns.FindByPredicate([&InAbilityTag, InPhase](const FWarriorBossPatternData& Pattern)
		{
			return Pattern.AbilityTag == InAbilityTag && Pattern.IsUsableInPhase(InPhase);
		});
}
