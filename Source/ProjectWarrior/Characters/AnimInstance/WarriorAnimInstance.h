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

	// 블레이드 모드 능력이 매 틱 넘겨줌. 각도는 BS_BladeAim 축(0 = 오른쪽, 90 = 위, -180~180)
	void SetBladeModeState(bool bInActive, float InAimAngle);

	// 베기 직후: 벤 각도의 자세(InFromAngle)에서 현재 각도의 자세로 InDuration초 동안 섞음
	void StartBladeSlashBlend(float InFromAngle, float InDuration);

protected:
	// 상체 레이어(BS_BladeAim)의 블렌드 가중치. 블레이드 모드가 켜지고 꺼질 때 0~1로 보간
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Warrior|Blade Mode")
	float BladeModeAlpha = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Warrior|Blade Mode")
	float BladeAimAngle = 0.0f;

	// 베기 직전 각도. 애님 그래프에서 이 각도의 자세(A)와 BladeAimAngle의 자세(B)를 BladeSlashBlendAlpha로 섞음
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Warrior|Blade Mode")
	float BladeSlashFromAngle = 0.0f;

	// 0 = 벤 자세, 1 = 현재 자세. 베지 않을 때는 1
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Warrior|Blade Mode")
	float BladeSlashBlendAlpha = 1.0f;

	// 블레이드 모드 자세로 들어가고 나오는 속도(초당 가중치)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Blade Mode")
	float BladeModeBlendSpeed = 8.0f;

	// 루트 모션 몽타주 재생 중 ALS 발 잠금(Foot Lock)과 발·골반 IK 오프셋을 끔.
	// ALS 발 잠금은 캡슐이 움직여도 발을 저장한 위치에 붙여 두므로, 루트 모션으로 이동하는 공격에서 다리가 늘어남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Foot IK")
	bool bDisableFootIKDuringRootMotionMontage = true;

	// 사망 상태(Shared.Status.Death)에서 발 잠금과 발·골반 IK 오프셋을 끔
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Foot IK")
	bool bDisableFootIKWhenDead = true;

private:
	void SuppressFootIK();

	bool bBladeModeActive = false;
	float BladeSlashBlendDuration = 0.0f;
};
