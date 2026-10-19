// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossLeapAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_BossJumpSlam.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 대상 위치로 뛰어올라 내리찍는 보스 도약 공격. 루트 모션이 없는 애니메이션용으로, 공중 이동은 코드(JumpForce)로 처리한다.
 * 몽타주 섹션: StartSection(도약 준비~이륙) -> AirSection(공중, 자기 자신으로 반복) -> LandSection(착지)
 * 흐름: 착지 지점·위험 범위 확정 -> 이륙 시점에 포물선 이동 -> 착지 섹션 + LandImpactDelay 후 판정
 * LandingRadii 개수만큼 연속으로 뛴다 (예: [400, 550] = 2회, 두 번째 착지 범위가 더 큼). 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossJumpSlam : public UAIGameplayAbility_BossLeapAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BossJumpSlam();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// Start -> Air(반복) -> Land 섹션을 가진 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam")
	TObjectPtr<UAnimMontage> LeapMontage;

	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam")
	FName StartSection = TEXT("Start");

	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam")
	FName AirSection = TEXT("Loop");

	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam")
	FName LandSection = TEXT("End");

	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;

	// StartSection 시작부터 이륙까지의 시간 (몽타주 시간, PlayRate 적용 전)
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam", meta = (ClampMin = "0.0", Units = "s"))
	float TakeoffTime = 0.4f;

	// 공중에 떠 있는 시간
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam", meta = (ClampMin = "0.1", Units = "s"))
	float AirDuration = 0.6f;

	// 포물선 최고 높이
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam", meta = (ClampMin = "0.0", Units = "cm"))
	float LeapHeight = 300.f;

	// LandSection 시작부터 판정까지의 시간 (몽타주 시간, PlayRate 적용 전)
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam", meta = (ClampMin = "0.0", Units = "s"))
	float LandImpactDelay = 0.05f;

	// 도약마다의 착지 범위 반지름. 개수 = 도약 횟수 (비어 있으면 DefaultAreaData로 1회)
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam")
	TArray<float> LandingRadii;

	// 연속 도약일 때 착지 판정 후 다음 도약 시작까지의 시간
	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam", meta = (ClampMin = "0.0", Units = "s"))
	float NextLeapDelay = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "JumpSlam|Damage")
	FScalableFloat DamageScalableFloat;

private:
	void StartLeap();

	UFUNCTION()
	void HandleTakeoff();

	UFUNCTION()
	void HandleLanded();

	UFUNCTION()
	void HandleLandImpact();

	UFUNCTION()
	void HandleNextLeap();

	UFUNCTION()
	void HandleFinished();

	UAnimInstance* GetOwnerAnimInstance() const;

	int32 GetLeapCount() const;

	int32 LeapIndex = 0;

	// 연속 도약 중 바꾼 DefaultAreaData.Radius를 되돌릴 값
	float SavedAreaRadius = 0.f;
	bool bRadiusOverridden = false;
};
