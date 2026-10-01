// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "Engine/EngineTypes.h"
#include "AIGameplayAbility_BossChargeAttack.generated.h"

class UAbilityTask_ApplyRootMotionMoveToForce;

enum class EBossChargePhase : uint8
{
	None,
	Tracking,	// 차징 중, 대상 방향으로 회전
	Locked,		// 차징 막바지, 방향 고정
	Dashing
};

/**
 * 제자리에서 차징하며 대상을 향해 회전한 뒤 직선으로 돌진하는 보스 공격. 돌진 경로에 닿은 대상은 1회씩 판정.
 * 사용 흐름 (BP): BeginCharge(Box 위험 범위가 보스를 따라 회전하며 차오름) -> 차징 몽타주 재생
 *               -> 차징이 끝나면 StartDash(돌진 몽타주 재생) -> OnChargeDashFinished에서 마무리 몽타주 -> EndAbility
 * 판정 영역은 DefaultAreaData의 Width·Height·bUnblockable을 쓰고, Shape(Box)·Anchor·Length는 돌진 거리로 자동 설정된다.
 * 돌진 이동은 루트 모션 소스(ApplyRootMotionMoveToForce)로 처리하므로 돌진 몽타주의 루트 모션은 덮어써진다.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossChargeAttack : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

protected:
	//~ Begin GameplayAbility Interface.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 차징 시작. 보스 앞으로 돌진 경로(Box)를 표시하고, 차징 동안 대상을 향해 회전 (표시도 함께 회전)
	// ChargeDuration - LockBeforeDashTime 이후에는 방향을 고정 (StartDash 직전 회피 여유)
	// TargetActor가 비어 있으면 AI 컨트롤러 Focus -> 블랙보드 TargetActor 순으로 찾음. 대상이 없으면 회전 없이 진행하고 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|ChargeAttack")
	bool BeginCharge(AActor* TargetActor, float ChargeDuration = 1.f);

	// 현재 바라보는 방향으로 돌진 시작. 경로를 내비메시 기준으로 보정하고 표시를 그 경로에 고정
	// 돌진 중 닿은 적대 대상에게 InDamageSpecHandle 적용 (대상마다 1회). 이미 돌진 중이면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|ChargeAttack")
	bool StartDash(const FGameplayEffectSpecHandle& InDamageSpecHandle, float DashDuration = 0.5f);

	// 돌진이 끝났을 때 (EndAbility로 중간에 끝나면 호출되지 않음)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Ability|ChargeAttack")
	void OnChargeDashFinished();

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability|ChargeAttack")
	bool IsDashing() const { return ChargePhase == EBossChargePhase::Dashing; }

	// 최대 돌진 거리 (보스 중심 이동 거리). 벽·낭떠러지가 있으면 그 앞까지로 줄어듦
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack", meta = (ClampMin = "0.0"))
	float DashDistance = 800.f;

	// 차징 중 대상 방향으로 회전하는 속도 (도/초, 0 = 즉시)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack", meta = (ClampMin = "0.0"))
	float TrackingRotationSpeed = 360.f;

	// 차징 종료 이 시간 전부터 방향 고정
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack", meta = (ClampMin = "0.0"))
	float LockBeforeDashTime = 0.2f;

	// 돌진 판정이 보스 캡슐 앞으로 더 뻗는 거리
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack", meta = (ClampMin = "0.0"))
	float DashHitForwardReach = 30.f;

	// 돌진 중 Pawn 충돌 무시 (대상을 밀어내지 않고 관통)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack")
	bool bIgnorePawnCollisionDuringDash = true;

	// 돌진 중에도 고정된 경로 표시를 유지 (false면 StartDash 시 제거)
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack")
	bool bKeepTelegraphDuringDash = true;

	// 돌진 끝 지점을 내비메시 위로 보정하고, 경로가 막혀 있으면 막힌 지점까지로 줄임
	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack|Navigation")
	bool bProjectDashToNavMesh = true;

	UPROPERTY(EditDefaultsOnly, Category = "ChargeAttack|Navigation", meta = (EditCondition = "bProjectDashToNavMesh"))
	FVector NavProjectExtent = FVector(100.f, 100.f, 300.f);

private:
	void TickCharge();
	void ScheduleNextTick();
	void StopTick();

	void RotateTowardTarget(float DeltaTime);

	// 직전 위치 -> 현재 위치 구간을 판정 (빠르게 지나쳐도 놓치지 않도록)
	void ApplyDashSweepDamage();

	UFUNCTION()
	void HandleDashMoveFinished();

	// bNotifyBlueprint: 정상 종료면 OnChargeDashFinished 호출
	void FinishDash(bool bNotifyBlueprint);

	void RestorePawnCollision();

	FWarriorAttackAreaData MakeDashAreaData(float InLength) const;

	EBossChargePhase ChargePhase = EBossChargePhase::None;

	TWeakObjectPtr<AActor> ChargeTarget;

	float ChargeElapsedTime = 0.f;
	float ChargeLockTime = 0.f;

	FTimerHandle ChargeTickHandle;

	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionMoveToForce> DashMoveTask;

	FGameplayEffectSpecHandle DashDamageSpecHandle;

	// 이번 돌진에서 이미 판정한 대상
	TSet<TWeakObjectPtr<AActor>> DashProcessedActors;

	FVector PreviousDashLocation = FVector::ZeroVector;
	FRotator DashRotation = FRotator::ZeroRotator;

	bool bAppliedCollisionIgnore = false;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
