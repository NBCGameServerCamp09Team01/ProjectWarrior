// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_Preparing.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_Preparing::UWarriorStageState_Preparing()
{
	StateType = EWarriorStageState::Preparing;
}

EWarriorStageState UWarriorStageState_Preparing::HandleEvent(EWarriorStageEvent InEvent)
{
	switch (InEvent)
	{
	case EWarriorStageEvent::StateTimerElapsed:
	{
		//M24: 서버 웨이브 구성이 아직 없으면 준비 상태에서 기다린다. GameMode가 한도를 재고, 구성이 오면 이 이벤트를 다시 보낸다
		AWarriorStageGameMode* StageGameMode = GetStageGameMode();
		if (StageGameMode && !StageGameMode->CanStartWaves())
		{
			StageGameMode->WaitForWavePlan();
			return EWarriorStageState::None;
		}
		return EWarriorStageState::InProgress;
	}
	case EWarriorStageEvent::PlayerDied:		return EWarriorStageState::StageFailed;
	default:									return EWarriorStageState::None;
	}
}

float UWarriorStageState_Preparing::GetDuration() const
{
	const AWarriorStageGameMode* StageGameMode = GetStageGameMode();

	return StageGameMode ? StageGameMode->GetPrepareTime() : 0.f;
}
