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

private:
	UFUNCTION()
	void HandleImpactEvent(FGameplayEventData Payload);

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
