// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossChargeAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_BossChargeSmash.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 차징 -> 직선 돌진 -> 끝에서 마무리 공격(범위 판정)까지 처리하는 돌진 공격. BP 그래프 없이 값만 설정해서 사용
 * ChargeMontage·DashMontage는 반복 섹션으로 만들어 두고, 시간은 ChargeDuration·DashDuration이 정한다.
 * 마무리 판정 시점은 FinishMontage에서 MontageImpactEventTag를 보내는 노티파이, 영역은 FinishAreaData
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossChargeSmash : public UAIGameplayAbility_BossChargeAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BossChargeSmash();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	virtual void OnChargeDashFinished_Implementation() override;

	// 차징 중 반복 재생 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash")
	TObjectPtr<UAnimMontage> ChargeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash", meta = (ClampMin = "0.1", Units = "s"))
	float ChargeDuration = 1.2f;

	// 돌진 중 반복 재생 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash", meta = (ClampMin = "0.1", Units = "s"))
	float DashDuration = 0.8f;

	// 돌진이 끝난 뒤 마무리 공격 몽타주 (선택, 없으면 돌진 후 바로 종료)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash")
	TObjectPtr<UAnimMontage> FinishMontage;

	// 마무리 공격 판정 영역 (판정 시점의 보스 위치·방향 기준)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash")
	FWarriorAttackAreaData FinishAreaData;

	// 돌진 경로에 닿았을 때의 피해
	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash|Damage")
	TSubclassOf<UGameplayEffect> DashDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash|Damage")
	FScalableFloat DashDamageScalableFloat;

	// 마무리 공격 피해
	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash|Damage")
	TSubclassOf<UGameplayEffect> FinishDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "ChargeSmash|Damage")
	FScalableFloat FinishDamageScalableFloat;

private:
	UFUNCTION()
	void HandleChargeFinished();

	UFUNCTION()
	void HandleFinishImpact(FGameplayEventData Payload);

	UFUNCTION()
	void HandleFinishMontageEnded();

	UFUNCTION()
	void HandleFinishMontageInterrupted();

	void PlayLoopMontage(UAnimMontage* Montage) const;

	void StopLoopMontages() const;
};
