// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ProjectWarrior/Types/WarriorStructTypes.h"
#include "WarriorAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	void OnAbilityInputPressed(const FGameplayTag& InputTag);
	void OnAbilityInputReleased(const FGameplayTag& InputTag);

public:
	//meta에 기본값을 지정할 변수를 넣고 값을 넣어주면 기본값 설정이 된다.
	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (ApplyLevel = "1"))
	void GrantPlayerWeaponAbilities(const TArray<FWarriorPlayerAbilitySet>& InDefaultWeaponAbilities, int32 ApplyLevel, TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilitySpecHandles);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	void RemovedGrantedHeroWeaponAbilities(UPARAM(ref) TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate);
};
