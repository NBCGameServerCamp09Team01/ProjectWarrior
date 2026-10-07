// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_RangeAttack.h"
#include "ScalableFloat.h"
#include "AIGameplayAbility_Snipe.generated.h"

class AWarriorAimBeam;
class AWarriorProjectileBase;
class UAnimMontage;
class UGameplayEffect;

/**
 * 조준선을 보여 주며 길게 조준한 뒤 강한 화살 한 발을 쏘는 원거리 엘리트 어빌리티.
 *  1. 화살을 손에 들고 저격 몽타주 재생 (AimSection을 반복하며 조준 유지), 조준선이 대상을 따라감
 *  2. AimDuration - LockLeadTime 시점에 조준 고정: 조준선 끝점 고정 + 고정 색 (이후 피하면 빗나감)
 *  3. AimDuration 시점에 FireSection으로 넘어가 FireEventTag 노티파이에서 고정 위치로 발사
 * 원거리 토큰은 GA BP의 AttackTokenPool로 지정. 처형·사망 시 취소된다.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_Snipe : public UAIGameplayAbility_RangeAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_Snipe();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 시위 당기기 -> 조준 유지(AimSection, 반복) -> 발사(FireSection) 구간으로 된 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Montage")
	TObjectPtr<UAnimMontage> SnipeMontage;

	// 조준을 유지하는 동안 반복할 구간 (몽타주에서 반복 연결이 없어도 이 재생에서만 반복)
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Montage")
	FName AimSection = FName("Aim");

	// 발사 동작 구간. 이 구간 안에 FireEventTag 노티파이가 있어야 함
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Montage")
	FName FireSection = FName("Fire");

	// 몽타주 노티파이 -> 화살 발사 시점
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Montage")
	FGameplayTag FireEventTag;

	// 조준 시작부터 발사 구간으로 넘어가기까지의 시간
	UPROPERTY(EditDefaultsOnly, Category = "Snipe", meta = (ClampMin = "0.1", Units = "s"))
	float AimDuration = 2.f;

	// 발사 구간으로 넘어가기 이 시간 전에 조준을 고정 (플레이어가 피할 수 있는 시간)
	UPROPERTY(EditDefaultsOnly, Category = "Snipe", meta = (ClampMin = "0.0", Units = "s"))
	float LockLeadTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Projectile")
	TSubclassOf<AWarriorProjectileBase> ProjectileClass;

	// 화살을 붙여 둘 소켓 (bAttachToCharacterMesh면 캐릭터 메시, 아니면 장착 무기 메시)
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Projectile")
	FName ProjectileSocketName;

	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Projectile")
	bool bAttachProjectileToCharacterMesh = false;

	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Projectile")
	TSubclassOf<UGameplayEffect> SnipeDamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Projectile")
	FScalableFloat SnipeDamage;

	// 비워 두면 조준선 없이 저격
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Beam")
	TSubclassOf<AWarriorAimBeam> AimBeamClass;

	// 조준선 시작 소켓. 비워 두면 ProjectileSocketName 사용
	UPROPERTY(EditDefaultsOnly, Category = "Snipe|Beam")
	FName AimBeamSocketName;

private:
	UFUNCTION()
	void OnLockAim();

	UFUNCTION()
	void OnStartFire();

	UFUNCTION()
	void OnFireEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnSnipeMontageEnded();

	UFUNCTION()
	void OnSnipeMontageInterrupted();

	void DestroyAimBeam();

	TWeakObjectPtr<AActor> SnipeTarget;
	TWeakObjectPtr<AWarriorAimBeam> AimBeam;
	bool bFired = false;
	bool bAimLocked = false;
};
