// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Animation/ALSCharacterAnimInstance.h"
#include "WarriorAnimInstance.generated.h"

/**
 *
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorAnimInstance : public UALSCharacterAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;

	virtual void NativeBeginPlay() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// 루트 모션 몽타주 재생 중 ALS 발 잠금(Foot Lock)과 발·골반 IK 오프셋을 끔.
	// ALS 발 잠금은 캡슐이 움직여도 발을 저장한 위치에 붙여 두므로, 루트 모션으로 이동하는 공격에서 다리가 늘어남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Foot IK")
	bool bDisableFootIKDuringRootMotionMontage = true;

	// 사망 상태(Shared.Status.Death)에서 발 잠금과 발·골반 IK 오프셋을 끔
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Foot IK")
	bool bDisableFootIKWhenDead = true;

private:
	void SuppressFootIK();
};
