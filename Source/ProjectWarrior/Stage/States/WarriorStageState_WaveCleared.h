// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_WaveCleared.generated.h"

/**
 * 웨이브 종료 연출. 타이머가 끝나면 마지막 웨이브였는지에 따라 클리어 또는 쉬는 시간으로 넘어간다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_WaveCleared : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_WaveCleared();

	virtual EWarriorStageState HandleEvent(EWarriorStageEvent InEvent) override;
	virtual bool UsesTimer() const override { return true; }
	virtual float GetDuration() const override;
};
