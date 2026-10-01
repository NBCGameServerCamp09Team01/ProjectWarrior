// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "Engine/EngineTypes.h"
#include "AIGameplayAbility_BossLeapAttack.generated.h"

/**
 * 대상에게 뛰어들어 착지 지점에 범위 판정하는 보스 점프 공격. 이동은 MotionWarping.
 * 사용 흐름 (BP): BeginLeap(착지 지점·Warp 목표·위험 범위를 한 번에 확정) -> 몽타주 재생
 *               -> 이륙 시 BeginLeapMovement -> 착지(AI.Event.Boss.AreaImpact) 시 EndLeapMovement + ApplyAreaDamage
 * 몽타주의 Motion Warping 노티파이 스테이트는 Warp Target Name을 WarpTargetName과 같게, Warp To Feet Location은 켠 상태로 둔다.
 * 판정 영역은 DefaultAreaData를 쓰고, Anchor 설정과 무관하게 BeginLeap 시점 위치에 고정된다.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossLeapAttack : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

protected:
	//~ Begin GameplayAbility Interface.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 호출 시점의 대상 위치로 착지 지점을 계산해 MotionWarping 목표와 위험 범위를 설정. 대상을 찾지 못하면 false
	// TargetActor가 비어 있으면 AI 컨트롤러 Focus -> 블랙보드 TargetActor 순으로 찾음
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|LeapAttack")
	bool BeginLeap(AActor* TargetActor, float TelegraphDuration);

	// 이륙 시점에 호출. 공중 구간 이동 모드·충돌 설정 (중복 호출 무시)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|LeapAttack")
	void BeginLeapMovement();

	// 착지 시점에 호출. BeginLeapMovement 이전 상태로 복구 (EndAbility에서도 자동 호출)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|LeapAttack")
	void EndLeapMovement();

	// BeginLeap에서 계산한 착지 지점 (발 위치 기준)
	UFUNCTION(BlueprintPure, Category = "Warrior|Ability|LeapAttack")
	FVector GetLeapLandingLocation() const { return LandingLocation; }

	// 몽타주 Motion Warping 노티파이 스테이트의 Warp Target Name과 같아야 함
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack")
	FName WarpTargetName = TEXT("LeapTarget");

	// 착지 시 보스와 대상 캡슐 표면 사이에 남길 거리
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack", meta = (ClampMin = "0.0"))
	float LandingGap = 50.f;

	// 최대 이동 거리 (0 = 제한 없음). 대상이 더 멀면 이 거리만큼만 뛰어감
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack", meta = (ClampMin = "0.0"))
	float MaxLeapDistance = 0.f;

	// true: 위험 범위 중심 = 착지 지점, false: BeginLeap 시점의 대상 위치
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack")
	bool bCenterAreaOnLanding = false;

	// 착지 지점을 내비메시 위로 보정하고, 보스 -> 착지 지점 사이가 막혀 있으면 막힌 지점까지로 줄임
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack|Navigation")
	bool bProjectLandingToNavMesh = true;

	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack|Navigation", meta = (EditCondition = "bProjectLandingToNavMesh"))
	FVector NavProjectExtent = FVector(100.f, 100.f, 300.f);

	// 공중 구간을 Flying 모드로 처리. Walking 모드에서는 루트 모션의 위아래 이동이 무시되므로,
	// 점프 높이가 루트 본에 들어 있는 애니메이션이면 켜야 함
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack|Movement")
	bool bUseFlyingDuringLeap = true;

	// 공중 구간에 Pawn 충돌을 무시 (플레이어에게 막히거나 밀어내지 않도록)
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack|Movement")
	bool bIgnorePawnCollisionDuringLeap = true;

	// 공중 구간에 ALS 상태를 InAir로 전환. ALS는 Flying 모드를 공중으로 보지 않아 Grounded로 남고,
	// 그 상태에서는 발 IK가 발을 바닥에 맞추려 해 다리가 늘어남. 착지 시 Grounded로 돌아감
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack|Movement")
	bool bSetALSInAirDuringLeap = true;

	// 이륙 시 착지 방향으로 즉시 회전. ALS InAir 상태는 진입 순간의 방향을 유지하려 하므로
	// 미리 맞춰 두어야 MotionWarping 회전과 충돌하지 않음
	UPROPERTY(EditDefaultsOnly, Category = "LeapAttack|Movement")
	bool bFaceLeapDirectionOnTakeoff = true;

private:
	// 내비메시 투영·장애물 확인을 거친 착지 지점. 실패하면 InDesiredLanding 그대로
	FVector AdjustLandingToNavigation(const FVector& InStartFeet, const FVector& InDesiredLanding) const;

	void RemoveLeapWarpTarget();

	FVector LandingLocation = FVector::ZeroVector;

	// BeginLeap에서 계산한 보스 -> 대상 방향 (MotionWarping 목표 회전과 같음)
	FRotator LeapRotation = FRotator::ZeroRotator;
	bool bHasLeapRotation = false;

	bool bAppliedFlying = false;
	bool bAppliedCollisionIgnore = false;
	bool bAppliedALSInAir = false;

	TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
	uint8 SavedCustomMovementMode = 0;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
