// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCostCalc.h"
#include "ProjectWarrior/AbilitySystem/Abilities/WarriorGameplayAbility.h"

float UBaseCostCalc::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const UWarriorGameplayAbility* WarriorAbility = Cast<UWarriorGameplayAbility>(Spec.GetContext().GetAbility());

	if (!WarriorAbility)
	{
		return 0.0f;
	}

	return WarriorAbility->GetCost();
}
