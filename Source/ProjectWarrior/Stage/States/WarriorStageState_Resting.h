// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_Resting.generated.h"

/**
 * 쉬는 시간. 상점을 쓸 수 있고, 타이머가 끝나거나 건너뛰면 다음 웨이브를 시작한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_Resting : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_Resting();

	virtual EWarriorStageState HandleEvent(EWarriorStageEvent InEvent) override;
	virtual bool UsesTimer() const override { return true; }
	virtual float GetDuration() const override;
};
