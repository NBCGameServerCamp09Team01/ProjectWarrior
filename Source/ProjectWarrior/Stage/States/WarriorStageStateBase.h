// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageStateBase.generated.h"

class AWarriorStageGameMode;

/**
 * 스테이지 상태 객체의 공통 부모. AWarriorStageGameMode가 상태마다 1개씩 만들어 소유한다.
 * 같은 함수(OnEnter, OnExit, HandleEvent)를 상태마다 다르게 재정의한다.
 * 반드시 NewObject<T>(GameMode)로 GameMode를 Outer로 지정해 생성한다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageStateBase : public UObject
{
	GENERATED_BODY()

public:
	EWarriorStageState GetStateType() const { return StateType; }

	//상태에 들어올 때 1회
	virtual void OnEnter(EWarriorStageState InPrevState) {}

	//상태에서 나갈 때 1회
	virtual void OnExit(EWarriorStageState InNextState) {}

	//이벤트를 받아 다음 상태를 돌려준다. 상태를 유지하면 None
	virtual EWarriorStageState HandleEvent(EWarriorStageEvent InEvent) { return EWarriorStageState::None; }

	//true면 GameMode가 GetDuration() 뒤에 StateTimerElapsed를 보낸다. 길이가 0 이하면 즉시 보낸다.
	virtual bool UsesTimer() const { return false; }

	virtual float GetDuration() const { return 0.f; }

protected:
	//생성 시 Outer로 지정된 GameMode. Outer가 GameMode가 아니면 nullptr
	AWarriorStageGameMode* GetStageGameMode() const;

	//자식 생성자에서 지정
	EWarriorStageState StateType = EWarriorStageState::None;
};
