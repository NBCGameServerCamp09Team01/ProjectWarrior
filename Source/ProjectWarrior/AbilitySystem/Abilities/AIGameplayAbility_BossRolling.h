// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_BossRolling.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;
class UAbilityTask_ApplyRootMotionConstantForce;
class UFXSystemAsset;
class UFXSystemComponent;

/**
 * 몸을 말아 빠르게 구르며 벽에 부딪힐 때마다 반사각으로 튕겨 나가고, 닿는 대상에게 피해를 주는 보스 공격.
 * 구르기가 끝나면 StunDuration 동안 기절한다 (기절 중 StunnedStatusTag 부여).
 * 흐름: RollMontage StartSection -> (RollStartDelay 후) LoopSection 반복 + 구르기 RollDuration -> StunMontage 반복 StunDuration -> 종료
 * 피해 영역: 보스 중심 원 (캡슐 반지름 + HitRadiusPadding), 같은 대상은 HitInterval마다 다시 맞을 수 있음. 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossRolling : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BossRolling();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 시작(몸 말기) -> 반복(구르기) 섹션을 가진 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Rolling")
	TObjectPtr<UAnimMontage> RollMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling")
	FName StartSection = TEXT("Start");

	UPROPERTY(EditDefaultsOnly, Category = "Rolling")
	FName LoopSection = TEXT("Loop");

	// 시작 섹션을 보여 준 뒤 굴러가기 시작할 때까지의 시간
	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.0", Units = "s"))
	float RollStartDelay = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.1", Units = "s"))
	float RollDuration = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RollSpeed = 1400.f;

	// 이 각도보다 가파른 면(벽)에 부딪히면 튕김 (0 = 수직 벽만)
	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
	float WallMaxSlopeFromVertical = 45.f;

	// 튕길 때 반사 방향에 더할 무작위 각도 (같은 길만 왕복하지 않도록)
	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.0", ClampMax = "60.0", Units = "deg"))
	float BounceRandomAngle = 12.f;

	// 벽 감지 거리 (캡슐 앞쪽으로 더 보는 거리)
	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "1.0", Units = "cm"))
	float WallProbeDistance = 40.f;

	// 피해 영역 반지름 = 캡슐 반지름 + 이 값
	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.0", Units = "cm"))
	float HitRadiusPadding = 60.f;

	// 같은 대상이 다시 맞을 수 있는 간격
	UPROPERTY(EditDefaultsOnly, Category = "Rolling", meta = (ClampMin = "0.1", Units = "s"))
	float HitInterval = 1.f;

	// 벽에 부딪힌 지점에 재생할 이펙트 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "Rolling|FX")
	TObjectPtr<UNiagaraSystem> BounceEffect;

	// 굴러가는 동안 몸에 붙여 계속 재생할 이펙트 (흙먼지 궤적·왜곡 등, Niagara/Cascade, 선택). 기절할 때 꺼짐
	UPROPERTY(EditDefaultsOnly, Category = "Rolling|FX")
	TObjectPtr<UFXSystemAsset> RollTrailEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling|FX")
	FVector RollTrailEffectScale = FVector::OneVector;

	// 구르기 후 기절 몽타주 (시작 -> 반복 섹션). 없으면 제자리에서 시간만 기다림
	UPROPERTY(EditDefaultsOnly, Category = "Rolling|Stun")
	TObjectPtr<UAnimMontage> StunMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling|Stun", meta = (ClampMin = "0.0", Units = "s"))
	float StunDuration = 5.f;

	// 기절 중 붙는 태그
	UPROPERTY(EditDefaultsOnly, Category = "Rolling|Stun")
	FGameplayTag StunnedStatusTag;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Rolling|Damage")
	FScalableFloat DamageScalableFloat;

private:
	UFUNCTION()
	void HandleRollStart();

	UFUNCTION()
	void HandleRollFinished();

	UFUNCTION()
	void HandleStunFinished();

	void TickRolling();

	// 현재 방향으로 남은 시간 동안 굴러가는 루트 모션 시작 (진행 중인 것은 교체)
	void StartRollMovement();

	void StopRollMovement();

	// 앞에 벽이 있으면 반사 방향으로 바꾸고 true
	bool TryBounce();

	void ApplyRollDamage();

	void SetStunned(bool bStunned);

	void RestorePawnCollision();

	UAnimInstance* GetOwnerAnimInstance() const;

	FTimerHandle RollTickHandle;

	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> RollMoveTask;

	UPROPERTY(Transient)
	TObjectPtr<UFXSystemComponent> RollTrailComponent;

	void StopRollTrail();

	FVector RollDirection = FVector::ForwardVector;

	float RollEndTime = 0.f;

	// 같은 대상 연속 판정 방지: 판정한 대상과 그 시각
	TSet<TWeakObjectPtr<AActor>> RollProcessedActors;
	TMap<TWeakObjectPtr<AActor>, float> RollHitTimes;

	FGameplayEffectSpecHandle RollDamageSpecHandle;

	bool bRolling = false;
	bool bAppliedStunTag = false;
	bool bAppliedCollisionIgnore = false;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
