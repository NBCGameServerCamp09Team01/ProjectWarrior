// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageStateBase.h"
#include "WarriorStageState_Initializing.generated.h"

/**
 * 레벨 시작 직후. GameMode가 플레이어 스폰과 FlowManager 등록을 확인하면 StageReady를 보낸다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageState_Initializing : public UWarriorStageStateBase
{
	GENERATED_BODY()

public:
	UWarriorStageState_Initializing();

	virtual EWarriorStageState HandleEvent(EWarriorStageEvent InEvent) override;
};
