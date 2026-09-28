// Fill out your copyright notice in the Description page of Project Settings.


#include "GEExecCalc_BalanceTaken.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/WarriorGameplayTags.h"

struct FWarriorBalanceCapture
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(DefensePower)
	DECLARE_ATTRIBUTE_CAPTUREDEF(BalanceDamageTaken)

	FWarriorBalanceCapture()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UWarriorAttributeSet, DefensePower, Source, false)
		DEFINE_ATTRIBUTE_CAPTUREDEF(UWarriorAttributeSet, BalanceDamageTaken, Target, false)
	}
};

static const FWarriorBalanceCapture& GetWarriorBalanceCapture()
{
	static FWarriorBalanceCapture WarriorBalanceCapture;
	return WarriorBalanceCapture;
}

UGEExecCalc_BalanceTaken::UGEExecCalc_BalanceTaken()
{
	RelevantAttributesToCapture.Add(FWarriorBalanceCapture().DefensePowerDef);
	RelevantAttributesToCapture.Add(FWarriorBalanceCapture().BalanceDamageTakenDef);
}

void UGEExecCalc_BalanceTaken::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& EffectSpec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = EffectSpec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParameters.TargetTags = EffectSpec.CapturedTargetTags.GetAggregatedTags();

	float SourceDefensePower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(GetWarriorBalanceCapture().DefensePowerDef, EvaluateParameters, SourceDefensePower);

	float BaseDamage = 0.f;

	for (const TPair<FGameplayTag, float>& TagMagnitude : EffectSpec.SetByCallerTagMagnitudes)
	{
		if (TagMagnitude.Key.MatchesTagExact(WarriorGameplayTags::Shared_SetByCaller_BaseDamage))
		{
			BaseDamage = TagMagnitude.Value;
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("BaseDamage : %02f"), BaseDamage));
		}
	}

	const float FinalBalanceDamageDone = BaseDamage * SourceDefensePower;

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("FinalBalanceDamageDone : %02f"), FinalBalanceDamageDone));

	if (FinalBalanceDamageDone > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				GetWarriorBalanceCapture().BalanceDamageTakenProperty,
				EGameplayModOp::Override,
				FinalBalanceDamageDone
			)
		);
	}
}
