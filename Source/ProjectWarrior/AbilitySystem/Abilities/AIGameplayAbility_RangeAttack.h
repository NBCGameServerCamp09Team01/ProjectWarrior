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

	// 들고 있는 투사체를 현재 위치에서 대상 방향으로 발사. TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터 사용
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability")
	bool LaunchHeldProjectile(AActor* TargetActor, const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bPredictTargetMovement = true);

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
	AWarriorProjectileBase* GetHeldProjectile() const;

	// 발사 전에 어빌리티가 끝나면(피격 등으로 취소) 들고 있던 투사체를 제거
	UPROPERTY(EditDefaultsOnly, Category = "RangeAttack")
	bool bDestroyHeldProjectileOnEnd = true;

private:
	void DestroyHeldProjectile();

	TWeakObjectPtr<AWarriorProjectileBase> HeldProjectile;
};
