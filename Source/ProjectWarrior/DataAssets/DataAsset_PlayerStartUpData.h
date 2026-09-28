// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset_StartUpDataBase.h"
#include "GameplayTagContainer.h"
#include "DataAsset_PlayerStartUpData.generated.h"

/**
 * 
 */

struct FWarriorPlayerAbilitySet;

UCLASS()
class PROJECTWARRIOR_API UDataAsset_PlayerStartUpData : public UDataAsset_StartUpDataBase
{
	GENERATED_BODY()
	
public:
    virtual void GiveToAbilitySystemComponent(UWarriorAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;

private:
    UPROPERTY(EditDefaultsOnly, Category = "StartUpData", meta = (TitleProperty = "InputTag"))
    TArray<FWarriorPlayerAbilitySet> HeroStartUpAbilitySets;
};
