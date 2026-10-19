// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "AIGameplayAbility_RangeAttack.generated.h"

class AWarriorProjectileBase;

/**
 * 활처럼 투사체를 소켓에 들고 있다가 원하는 타이밍에 발사하는 AI 원거리 공격
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_RangeAttack : public UAIGameplayAbility
{
	GENERATED_BODY()

protected:
	//~ Begin GameplayAbility Interface.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 투사체를 스폰해 소켓에 붙여 둠 (충돌/이동 꺼진 대기 상태). 이미 들고 있으면 기존 것을 교체
	// bAttachToCharacterMesh가 false면 장착 무기 메시의 소켓, true면 캐릭터 메시의 소켓(손 등)에 붙임
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability", meta = (DeterminesOutputType = "ProjectileClass"))
	AWarriorProjectileBase* SpawnHeldProjectile(TSubclassOf<AWarriorProjectileBase> ProjectileClass, FName AttachSocketName, bool bAttachToCharacterMesh = false);

	// 들고 있는 투사체를 현재 위치에서 발사. TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터 사용
	// LockAimLocation 이후 대상이 회피했으면(또는 bOnlyUseLockedAimWhenDodged가 false면) 고정 위치로,
	// 아니면 대상의 현재 위치(+ bPredictTargetMovement면 이동 예측)로 발사
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability")
	bool LaunchHeldProjectile(AActor* TargetActor, const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bPredictTargetMovement = true);

	// 호출 시점의 대상 위치를 고정 조준점으로 저장하고 대상의 회피(Shared.Status.Dodge) 감시 시작
	// TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터 사용. 대상이 없으면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability")
	bool LockAimLocation(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability")
	void ClearLockedAimLocation();

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
	AWarriorProjectileBase* GetHeldProjectile() const;

	// 발사 전에 어빌리티가 끝나면(피격 등으로 취소) 들고 있던 투사체를 제거
	UPROPERTY(EditDefaultsOnly, Category = "RangeAttack")
	bool bDestroyHeldProjectileOnEnd = true;

	// true: 고정 후 대상이 회피했을 때만 고정 위치로 발사 (걷기는 추적, 회피는 빗나감)
	// false: LockAimLocation을 호출했으면 항상 고정 위치로 발사
	UPROPERTY(EditDefaultsOnly, Category = "RangeAttack")
	bool bOnlyUseLockedAimWhenDodged = true;

	// 회피했을 때 고정 조준점을 회피한 위치 쪽으로 당겨오는 비율 (0 = 고정 위치 그대로, 1 = 최소 빗나감 거리까지)
	UPROPERTY(EditDefaultsOnly, Category = "RangeAttack", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DodgeAimCorrectionRatio = 0.5f;

	// 회피했을 때 화살 궤적이 대상에게서 최소한 떨어질 거리 (궤적에 수직 방향 기준, 캡슐 반지름 + 여유)
	UPROPERTY(EditDefaultsOnly, Category = "RangeAttack", meta = (ClampMin = "0.0"))
	float MinDodgeMissDistance = 80.f;

private:
	// 회피한 대상 쪽으로 보정하되 MinDodgeMissDistance 이상 빗나가는 조준점
	FVector ComputeDodgeCorrectedAimLocation(const FVector& LaunchLocation, const FVector& TargetLocation) const;

	void DestroyHeldProjectile();

	void OnLockedTargetDodgeTagChanged(const FGameplayTag Tag, int32 NewCount);
	void StopWatchingLockedTarget();

	TWeakObjectPtr<AWarriorProjectileBase> HeldProjectile;

	bool bHasLockedAimLocation = false;
	FVector LockedAimLocation = FVector::ZeroVector;

	// LockAimLocation 이후 대상이 회피를 시작했는지
	bool bLockedTargetDodged = false;

	TWeakObjectPtr<AActor> LockedTarget;
	TWeakObjectPtr<UAbilitySystemComponent> LockedTargetASC;
	FDelegateHandle DodgeTagDelegateHandle;
};
