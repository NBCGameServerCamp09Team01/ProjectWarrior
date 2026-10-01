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

    // 현재 무기 충돌 구간의 히트리액션 강도(Shared.Event.HitReact.Light/Heavy). 무기 충돌 노티파이가 켤 때 설정하고,
    // 충돌을 끄면 비워짐. 비어 있으면 공격 어빌리티의 기본 강도를 사용
    UFUNCTION(BlueprintCallable, Category = "Warrior|Combat")
    void SetCurrentHitReactEventTag(UPARAM(meta = (Categories = "Shared.Event.HitReact")) FGameplayTag InHitReactEventTag);

    UFUNCTION(BlueprintPure, Category = "Warrior|Combat")
    FGameplayTag GetCurrentHitReactEventTag() const { return CurrentHitReactEventTag; }

    virtual void OnHitTargetActor(AActor* HitActor);
    virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor);

protected:
    TArray<AActor*> OverlappedActors;

    FGameplayTag CurrentHitReactEventTag;


private:
    TMap<FGameplayTag, AWeaponBase*> CharacterCarriedWeaponMap;
};
