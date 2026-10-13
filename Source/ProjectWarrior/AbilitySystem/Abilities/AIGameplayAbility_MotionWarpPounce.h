// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossLeapAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_MotionWarpPounce.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 대상에게 달려들어 물거나 할퀴는 공격. 루트 모션 몽타주를 MotionWarping(Skew Warp)으로 늘려 대상 앞까지 이동한다.
 * Warp 구간은 몽타주에 노티파이 스테이트를 넣지 않고 발동 시 직접 추가하므로, 판정 노티파이(MontageImpactEventTag)만 있으면 된다.
 * 착지 지점·위험 범위 계산은 부모(UAIGameplayAbility_BossLeapAttack::BeginLeap)를 사용. 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_MotionWarpPounce : public UAIGameplayAbility_BossLeapAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_MotionWarpPounce();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End GameplayAbility Interface

	// 루트 모션이 켜진 공격 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Pounce")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Pounce", meta = (ClampMin = "0.1"))
	float AttackPlayRate = 1.f;

	// Warp 구간 시작 (몽타주 시간)
	UPROPERTY(EditDefaultsOnly, Category = "Pounce", meta = (ClampMin = "0.0", Units = "s"))
	float WarpStartTime = 0.f;

	// Warp 구간 끝 (몽타주 시간). 0 이하면 판정 노티파이 시점
	UPROPERTY(EditDefaultsOnly, Category = "Pounce", meta = (Units = "s"))
	float WarpEndTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Pounce|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Pounce|Damage")
	FScalableFloat DamageScalableFloat;

private:
	UFUNCTION()
	void HandleImpactEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();
};
