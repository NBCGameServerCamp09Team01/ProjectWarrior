// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_StageCleared.generated.h"

/**
 * 마지막(보스) 웨이브 클리어. 끝 상태라 이후 이벤트는 무시한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_StageCleared : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_StageCleared();

	virtual void OnEnter(EWarriorStageState InPrevState) override;
};
