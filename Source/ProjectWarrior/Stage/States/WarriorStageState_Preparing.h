// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_Preparing.generated.h"

/**
 * 준비 시간(최초 쉬는 시간). 타이머가 끝나면 첫 웨이브를 시작한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_Preparing : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_Preparing();

	virtual EWarriorStageState HandleEvent(EWarriorStageEvent InEvent) override;
	virtual bool UsesTimer() const override { return true; }
	virtual float GetDuration() const override;
};
