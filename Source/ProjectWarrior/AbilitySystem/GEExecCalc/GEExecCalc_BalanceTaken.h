// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GEExecCalc_BalanceTaken.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UGEExecCalc_BalanceTaken : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
public:
	UGEExecCalc_BalanceTaken();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
