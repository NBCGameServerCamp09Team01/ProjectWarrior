// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_InProgress.generated.h"

/**
 * 웨이브 진행. 들어올 때 다음 웨이브를 시작하고, 적이 전멸하면 웨이브 종료로 넘어간다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_InProgress : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_InProgress();

	virtual void OnEnter(EWarriorStageState InPrevState) override;
	virtual EWarriorStageState HandleEvent(EWarriorStageEvent InEvent) override;
};
