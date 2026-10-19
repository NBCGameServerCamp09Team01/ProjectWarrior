// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_StageFailed.generated.h"

/**
 * 플레이어 사망. 끝 상태라 이후 이벤트는 무시한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_StageFailed : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_StageFailed();

	virtual void OnEnter(EWarriorStageState InPrevState) override;
};
