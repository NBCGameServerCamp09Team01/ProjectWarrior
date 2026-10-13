// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_ProjectileVolley.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;
class AWarriorProjectileBase;
class UFXSystemAsset;

/**
 * 몽타주를 재생하고, 발사 노티파이(FireEventTag)마다 캐릭터 메시의 뼈/소켓에서 투사체를 부채꼴로 발사하는 공격.
 * 무기가 없는 몬스터(침 뱉기 등)용. 몽타주 재생, 피해 적용, 종료까지 이 클래스가 처리하므로 BP 그래프 없이 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_ProjectileVolley : public UAIGameplayAbility
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_ProjectileVolley();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	TSubclassOf<AWarriorProjectileBase> ProjectileClass;

	// 몽타주에서 이 태그를 보내는 노티파이 시점마다 한 번 발사
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	FGameplayTag FireEventTag;

	// 비어 있지 않으면 이 노티파이 시점에 투사체를 발사 위치(뼈/소켓)에 붙여 들고 있다가, 다음 발사 노티파이에서 그 자리에서 발사
	// (바위를 뜯어 들고 던지기 등). 들고 있는 투사체가 없을 때의 발사는 평소처럼 새로 생성
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	FGameplayTag HoldEventTag;

	// 들고 있을 때 뼈/소켓 기준 위치 오프셋
	UPROPERTY(EditDefaultsOnly, Category = "Volley", meta = (EditCondition = "HoldEventTag.IsValid()"))
	FVector HoldOffset = FVector::ZeroVector;

	// 들고 있는 투사체가 캐릭터 메시 크기를 따라감 (레벨에서 키운 보스가 든 바위도 같이 커짐)
	UPROPERTY(EditDefaultsOnly, Category = "Volley", meta = (EditCondition = "HoldEventTag.IsValid()"))
	bool bHoldInheritsOwnerScale = false;

	// 들어 올리는 순간 땅에서 재생할 이펙트 (바위를 뜯는 흙 등, Niagara/Cascade, 선택)
	UPROPERTY(EditDefaultsOnly, Category = "Volley|FX")
	TObjectPtr<UFXSystemAsset> HoldGroundEffect;

	// HoldGroundEffect 위치로 쓸 소켓 (높이는 발밑). 비어 있으면 들고 있는 뼈/소켓 아래
	UPROPERTY(EditDefaultsOnly, Category = "Volley|FX")
	FName HoldGroundEffectSocket;

	// 던지는 순간 투사체 위치에서 재생할 이펙트 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "Volley|FX")
	TObjectPtr<UFXSystemAsset> ThrowEffect;

	// 발사 위치로 쓸 캐릭터 메시의 뼈/소켓. 발사할 때마다 순서대로 돌아가며 사용 (좌 -> 우 -> 좌 ...)
	// 비어 있거나 찾지 못하면 캐릭터 정면 100cm
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	TArray<FName> SpawnBoneNames;

	// 한 번 발사할 때의 투사체 수
	UPROPERTY(EditDefaultsOnly, Category = "Volley", meta = (ClampMin = "1"))
	int32 ProjectilesPerFire = 1;

	// 투사체가 2개 이상일 때 좌우로 펼칠 전체 각도
	UPROPERTY(EditDefaultsOnly, Category = "Volley", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float SpreadAngle = 30.f;

	// 대상 이동을 예측해서 조준 (부채꼴의 중심 방향에만 적용)
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	bool bPredictTargetMovement = true;

	// 직선 대신 포물선으로 대상 위치에 떨어지게 던짐 (투사체 ProjectileMovement의 중력 배율이 0보다 커야 함)
	// 투사체 속도(InitialSpeed)로 닿는 각도를 계산하고, 너무 멀어 닿지 않으면 ArcFallbackCurve 높이의 포물선으로 속도를 맞춰 던짐
	UPROPERTY(EditDefaultsOnly, Category = "Volley|Arc")
	bool bUseArcTrajectory = false;

	// 같은 속도로 닿는 두 각도 중 높은 쪽을 쓸지 (높으면 오래 떠 있어 피하기 쉬움)
	UPROPERTY(EditDefaultsOnly, Category = "Volley|Arc", meta = (EditCondition = "bUseArcTrajectory"))
	bool bFavorHighArc = false;

	// 속도로 닿지 않을 때 쓰는 포물선 높이 (0 = 직선에 가까움, 1 = 매우 높음)
	UPROPERTY(EditDefaultsOnly, Category = "Volley|Arc", meta = (EditCondition = "bUseArcTrajectory", ClampMin = "0.05", ClampMax = "0.95"))
	float ArcFallbackCurve = 0.4f;

	// 몽타주 시작 시 대상 방향으로 즉시 회전
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	bool bFaceTargetOnStart = true;

	// 발사할 때마다 대상 방향으로 다시 회전
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	bool bFaceTargetOnFire = false;

	// 발사 위치에 붙여 재생할 이펙트 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "Volley|FX")
	TObjectPtr<UNiagaraSystem> MuzzleEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Volley|FX")
	FVector MuzzleEffectScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, Category = "Volley|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Volley|Damage")
	FScalableFloat DamageScalableFloat;

private:
	UFUNCTION()
	void HandleFireEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleHoldEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	void FaceTarget();

	FVector GetSpawnLocation(FName& OutBoneName) const;

	// 발사 방향과 속도 (bUseArcTrajectory면 포물선 계산, 아니면 직선 조준 + InitialSpeed)
	void ComputeLaunch(const FVector& LaunchLocation, AActor* TargetActor, FVector& OutDirection, float& OutSpeed);

	int32 FireCount = 0;

	// HoldEventTag로 들고 있는 (아직 발사하지 않은) 투사체. 먼저 든 것부터 발사
	TArray<TWeakObjectPtr<AWarriorProjectileBase>> HeldProjectiles;
};
