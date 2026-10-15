// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_BossMultiAreaAttack.generated.h"

class UGameplayEffect;

/**
 * 몽타주의 판정 노티파이(MontageImpactEventTag)가 여러 개인 범위 공격 (예: 2연속 쓸기).
 * 1타 범위 표시 -> 1타 판정 -> 바로 2타 범위 표시 -> 2타 판정 ... 순으로 진행한다.
 * 각 표시는 루트 모션을 미리 계산해 그 판정 시점의 보스 위치·방향에 고정된다.
 * 몽타주 재생, 피해 적용, 종료까지 이 클래스가 처리하므로 BP 그래프 없이 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossMultiAreaAttack : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BossMultiAreaAttack();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End GameplayAbility Interface

	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack")
	TObjectPtr<UAnimMontage> AttackMontage;

	// 판정마다 적용할 피해 효과와 피해량
	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack|Damage")
	FScalableFloat DamageScalableFloat;

	// 판정 순서별 영역. 비어 있거나 개수가 모자라면 DefaultAreaData 사용
	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack")
	TArray<FWarriorAttackAreaData> HitAreas;

	// 첫 표시 전에 대상 방향으로 회전
	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack")
	bool bFaceTargetOnStart = true;

	// 2타부터 표시 직전에도 대상 방향으로 회전 (끄면 애니메이션 방향 그대로)
	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack")
	bool bFaceTargetBeforeEachHit = false;

	// 범위 판정·표시 대신 장착 무기(투명 히트박스)의 충돌로 판정. 몽타주의 ANS_ToggleWeaponCollision 구간에 닿은 대상에게 피해
	// (잡몹 일반 공격처럼 피해야 할 범위 표시가 어울리지 않는 공격용)
	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack")
	bool bUseWeaponCollisionHit = false;

	// 무기 충돌 판정에 쓸 무기(등록 태그). 발동 시 장착 무기로 바꿔서 노티파이가 이 무기의 충돌을 켜게 함 (왼발·오른발처럼 여러 개일 때). 비우면 현재 장착 무기
	UPROPERTY(EditDefaultsOnly, Category = "MultiAreaAttack", meta = (Categories = "AI.Weapon", EditCondition = "bUseWeaponCollisionHit"))
	FGameplayTag HitWeaponTag;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleImpactEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleWeaponHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	// HitIndex번째 판정의 범위 표시. 판정이 더 없으면 false
	bool TelegraphHit(int32 HitIndex, bool bFaceTarget);

	const FWarriorAttackAreaData& GetHitArea(int32 HitIndex) const;

	// 몽타주의 판정 시간 (시간순)
	TArray<float> ImpactTimes;

	int32 CurrentHitIndex = 0;
};
