// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "WarriorGameplayAbility.generated.h"

class UPawnCombatComponent;
class UWarriorAbilitySystemComponent;

UENUM(BlueprintType)
enum class EWarriorAbilityActivationPolicy :uint8
{
	OnTriggered,
	OnGiven
};
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
protected:
	//~ Begin GameplayAbility Interface.
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec);
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled);
	//~ End GameplayAbility Interface


	UPROPERTY(EditDefaultsOnly, Category="WarriorAbility")
	EWarriorAbilityActivationPolicy AbilityActivationPolicy = EWarriorAbilityActivationPolicy::OnTriggered;

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
	UPawnCombatComponent* GetPawnCombatComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
	UWarriorAbilitySystemComponent* GetWarriorAbilitySystemComponentFromActorInfo() const;

	FActiveGameplayEffectHandle NativeApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability", meta = (DisplayName = "Apply Gameplay Effect Spec Handle To Target Actor", ExpandEnumAsExecs = "OutSuccessType"))
	FActiveGameplayEffectHandle BP_ApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle, EWarriorSuccessType& OutSuccessType);

	UPROPERTY(EditDefaultsOnly)
	float Cost;

	UPROPERTY(EditDefaultsOnly)
	float Cooldown;

public:
	FORCEINLINE float GetCost() const { return Cost; }
	FORCEINLINE float GetCooldown() const { return Cooldown; }
};
