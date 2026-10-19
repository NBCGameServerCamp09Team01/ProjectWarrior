// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayerGameplayAbility.h"
#include "PlayerGameplayAbility_Knockdown.generated.h"

class UAnimMontage;

/**
 * 넉백 피격(Shared.Event.HitReact.KnockBack) 시 넘어졌다가 일어나는 플레이어 어빌리티.
 * 공격자가 앞쪽이면 FrontHitMontage, 뒤쪽이면 BackHitMontage를 재생하고, 공격자를 정면 또는 정후면에 두도록 몸을 맞춘다.
 * 몽타주 구성: StartSection(넘어짐, 루트 모션으로 밀려남) -> LoopSection(누운 상태, DownDuration 동안 반복) -> EndSection(일어남)
 * 누워 있는 동안 무적(Shared.Status.Invulnerable). 넘어지는 중·일어나는 중 무적은 옵션
 */
UCLASS()
class PROJECTWARRIOR_API UPlayerGameplayAbility_Knockdown : public UPlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerGameplayAbility_Knockdown();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 앞에서 맞았을 때 (뒤로 넘어짐)
	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Montage")
	TObjectPtr<UAnimMontage> FrontHitMontage;

	// 뒤에서 맞았을 때 (앞으로 넘어짐)
	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Montage")
	TObjectPtr<UAnimMontage> BackHitMontage;

	// 두 몽타주 공통 구간 이름
	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Montage")
	FName StartSection = FName("Start");

	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Montage")
	FName LoopSection = FName("Loop");

	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Montage")
	FName EndSection = FName("End");

	// 넘어진 뒤 누워 있는 시간 (Start 구간이 끝난 뒤부터)
	UPROPERTY(EditDefaultsOnly, Category = "Knockdown", meta = (ClampMin = "0.0", Units = "s"))
	float DownDuration = 1.f;

	// 공격자를 정면(앞에서 맞음) 또는 정후면(뒤에서 맞음)에 두도록 몸을 돌림. 끄면 지금 방향 그대로 넘어짐
	UPROPERTY(EditDefaultsOnly, Category = "Knockdown")
	bool bAlignToAttacker = true;

	// 누워 있는 동안은 항상 무적. 아래는 넘어지는 중·일어나는 중 무적 여부
	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Invulnerable")
	bool bInvulnerableWhileFalling = true;

	UPROPERTY(EditDefaultsOnly, Category = "Knockdown|Invulnerable")
	bool bInvulnerableWhileGettingUp = true;

private:
	UFUNCTION()
	void OnDown();

	UFUNCTION()
	void OnGetUp();

	UFUNCTION()
	void OnKnockdownMontageEnded();

	UFUNCTION()
	void OnKnockdownMontageInterrupted();

	UFUNCTION()
	void OnOwnerDied();

	void SetInvulnerable(bool bEnable);
	void FaceDirection(const FVector& Direction) const;

	TWeakObjectPtr<UAnimMontage> PlayingMontage;
	bool bInvulnerable = false;
};
