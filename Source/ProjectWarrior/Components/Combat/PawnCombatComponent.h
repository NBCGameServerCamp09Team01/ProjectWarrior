// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
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

    // 현재 무기 충돌 구간의 막기 규칙. 막기 규칙 노티파이(UAnimNotifyState_WarriorBlockRule)가 설정하고,
    // 노티파이가 끝나거나 충돌을 끄면 Blockable로 돌아감
    // bStaggerOnBlocked: 이 구간의 공격이 막히면 공격자가 스태거(Shared.Event.Stagger)
    void SetCurrentBlockRule(EWarriorBlockRule InBlockRule, bool bInStaggerOnBlocked);
    void ResetCurrentBlockRule();

    EWarriorBlockRule GetCurrentBlockRule() const { return CurrentBlockRule; }

    virtual void OnHitTargetActor(AActor* HitActor);
    virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor);

protected:
    TArray<AActor*> OverlappedActors;

    FGameplayTag CurrentHitReactEventTag;

    EWarriorBlockRule CurrentBlockRule = EWarriorBlockRule::Blockable;

    bool bStaggerOnBlocked = false;


private:
    TMap<FGameplayTag, AWeaponBase*> CharacterCarriedWeaponMap;
};
