// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset_StartUpDataBase.h"
#include "DataAsset_AIStartUpData.generated.h"

class UAIGameplayAbility;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UDataAsset_AIStartUpData : public UDataAsset_StartUpDataBase
{
	GENERATED_BODY()
	
public:
    virtual void GiveToAbilitySystemComponent(UWarriorAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;

private:
    UPROPERTY(EditDefaultsOnly, Category = "StartUpData")
    TArray<TSubclassOf<UAIGameplayAbility>> AICombatAbilities;
};
