// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "PawnCombatComponent.generated.h"

class AWeaponBase;

UENUM(BlueprintType)
enum class EToggleDamageType : uint8
{
    CurrentEquippedWeapon,
    LeftHand,
    RightHand
};
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UPawnCombatComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = "Warrior|Combat")
    void RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, AWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon = false);

    UFUNCTION(BlueprintCallable, Category = "Warrior|Combat")
    AWeaponBase* GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const;

    UPROPERTY(BlueprintReadWrite, Category = "Warrior|Combat")
    FGameplayTag CurrentEquippedWeaponTag;

    UFUNCTION(BlueprintCallable, Category = "Warrior|Combat")
    AWeaponBase* GetCharacterCurrentEquippedWeapon() const;

    UFUNCTION(BlueprintCallable, Category = "Warrior|Combat")
    void ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType = EToggleDamageType::CurrentEquippedWeapon);

    virtual void OnHitTargetActor(AActor* HitActor);
    virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor);

protected:
    TArray<AActor*> OverlappedActors;


private:
    TMap<FGameplayTag, AWeaponBase*> CharacterCarriedWeaponMap;
};
