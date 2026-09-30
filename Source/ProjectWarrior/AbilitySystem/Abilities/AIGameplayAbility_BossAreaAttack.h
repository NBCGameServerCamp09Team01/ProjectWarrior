// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "ProjectWarrior/Types/WarriorAttackAreaTypes.h"
#include "AIGameplayAbility_BossAreaAttack.generated.h"

class AWarriorAttackIndicator;

/**
 * 위험 범위를 표시한 뒤 그 영역에 판정하는 보스 범위 공격.
 * 사용 흐름 (BP): 몽타주 재생 -> AI.Event.Boss.Telegraph 수신 시 BeginAreaTelegraph
 *               -> AI.Event.Boss.AreaImpact 수신 시 ApplyAreaDamage
 * 표시와 판정이 같은 FWarriorAttackAreaData / 영역 트랜스폼을 사용하므로 보이는 범위 = 맞는 범위.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossAreaAttack : public UAIGameplayAbility
{
	GENERATED_BODY()

protected:
	//~ Begin GameplayAbility Interface.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 영역 위치를 확정하고 위험 범위를 표시. 이미 표시 중이면 교체
	// TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터, 그것도 없으면 블랙보드 TargetActor 사용
	// (Anchor가 TargetSnapshot인데 대상을 찾지 못하면 공격자 기준 + 경고 로그)
	// TelegraphDuration은 표시가 가득 차는 시간 (판정 시점까지 남은 시간과 맞춤)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	void BeginAreaTelegraph(const FWarriorAttackAreaData& InAreaData, AActor* TargetActor, float TelegraphDuration);

	// DefaultAreaData로 BeginAreaTelegraph
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	void BeginDefaultAreaTelegraph(AActor* TargetActor, float TelegraphDuration);

	// 현재 영역 안의 적대 대상에게 판정. 피격(Hit)된 대상 목록 반환. 영역이 없으면 빈 목록
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	TArray<AActor*> ApplyAreaDamage(const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bClearTelegraph = true);

	// 표시를 제거하고 영역을 비움
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	void ClearAreaTelegraph();

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability|AreaAttack")
	bool HasActiveArea() const { return bHasActiveArea; }

	// Owner 앵커면 현재 공격자 기준, 그 외에는 고정된 트랜스폼
	UFUNCTION(BlueprintPure, Category = "Warrior|Ability|AreaAttack")
	FTransform GetCurrentAreaTransform() const;

	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	FWarriorAttackAreaData DefaultAreaData;

	// 비워 두면 표시 없이 판정만 함
	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	TSubclassOf<AWarriorAttackIndicator> IndicatorClass;

	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	bool bSendHitReactEvent = true;

	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack|Debug")
	bool bDrawDebugArea = false;

private:
	// 앵커 기준(오프셋 적용 전) 트랜스폼. 수평 방향만 사용
	FTransform ComputeAnchorTransform(EWarriorAttackAreaAnchor InAnchor, AActor* TargetActor);

	FWarriorAttackAreaData CurrentAreaData;

	// Owner 앵커가 아닐 때 고정된 영역 트랜스폼
	FTransform SnapshotAreaTransform;

	bool bHasActiveArea = false;

	TWeakObjectPtr<AWarriorAttackIndicator> ActiveIndicator;
};
