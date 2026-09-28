// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCooldownCalc.h"
#include "ProjectWarrior/AbilitySystem/Abilities/WarriorGameplayAbility.h"

float UBaseCooldownCalc::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const UWarriorGameplayAbility* WarriorAbility = Cast<UWarriorGameplayAbility>(Spec.GetContext().GetAbility());

	if (!WarriorAbility)
	{
		return 0.1f;
	}

	return WarriorAbility->GetCooldown();
}
