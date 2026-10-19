// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacterMovementComponent.h"
#include "WarriorCharacterMovementComponent.generated.h"

/**
 * 이동 속도 배율(신속 수식어, 냉기 둔화 등)을 최종 최고 속도에 곱하는 이동 컴포넌트. AWarriorBaseCharacter가 ALS 컴포넌트 대신 쓴다.
 * MaxWalkSpeed는 ALS(걸음·회전 모드가 바뀔 때)와 게임 코드(락온 150 등)가 각자 직접 쓰므로, 그 값을 바꾸지 않고 GetMaxSpeed 결과에만 곱한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorCharacterMovementComponent : public UALSCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// 땅에서 걷는 속도에만 곱한다(점프·낙하 속도는 그대로)
	virtual float GetMaxSpeed() const override;

	void SetSpeedScale(float InScale) { SpeedScale = FMath::Max(InScale, 0.f); }
	float GetSpeedScale() const { return SpeedScale; }

private:
	float SpeedScale = 1.f;
};
