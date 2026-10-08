// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnCombatComponent.h"
#include "PlayerCombatComponent.generated.h"

/**
 * 
 */
class AWeaponBase;
class UGameplayAbility;

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

	// 회피 중(Shared.Status.Dodge)인 대상은 맞지 않고, 정면 가드 중(AI.Status.Guarding)인 대상은 막힘
	virtual void OnHitTargetActor(AActor* HitActor) override;
	virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 공격 어빌리티(Player.Ability.Attack)가 발동하면 이 범위 안의 적에게 AI.Event.IncomingAttack을 보냄 (적의 회피 반응용)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|IncomingAttack", meta = (ClampMin = "0.0", Units = "cm"))
	float IncomingAttackRadius = 450.f;

	// 플레이어 정면 기준 좌우 각도. 이 안에 있는 적에게만 알림
	UPROPERTY(EditDefaultsOnly, Category = "Combat|IncomingAttack", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float IncomingAttackHalfAngle = 75.f;

private:
	void HandleAbilityActivated(UGameplayAbility* ActivatedAbility);
	void NotifyIncomingAttack() const;

	FDelegateHandle AbilityActivatedHandle;
};
