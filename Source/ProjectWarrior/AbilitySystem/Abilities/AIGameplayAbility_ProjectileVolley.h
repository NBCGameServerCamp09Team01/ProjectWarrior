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
	//~ End GameplayAbility Interface

	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	TSubclassOf<AWarriorProjectileBase> ProjectileClass;

	// 몽타주에서 이 태그를 보내는 노티파이 시점마다 한 번 발사
	UPROPERTY(EditDefaultsOnly, Category = "Volley")
	FGameplayTag FireEventTag;

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
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	void FaceTarget();

	FVector GetSpawnLocation(FName& OutBoneName) const;

	int32 FireCount = 0;
};
