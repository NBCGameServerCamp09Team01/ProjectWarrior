// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnCombatComponent.h"
#include "PlayerCombatComponent.generated.h"

/**
 * 
 */
class AWeaponBase;

UCLASS()
class PROJECTWARRIOR_API UPlayerCombatComponent : public UPawnCombatComponent
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Combat")
	AWeaponBase* GetCarriedWeaponByTag(FGameplayTag InWeaponTag) const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	AWeaponBase* GetCurrentEquippedWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	float GetPlayerCurrentEquippedWeaponDamageAtLevel(float InLevel) const;

	virtual void OnHitTargetActor(AActor* HitActor) override;
	virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor) override;
};
