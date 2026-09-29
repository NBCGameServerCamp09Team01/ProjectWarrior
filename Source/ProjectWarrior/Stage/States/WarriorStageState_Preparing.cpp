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
	case EWarriorStageEvent::StateTimerElapsed:	return EWarriorStageState::InProgress;
	case EWarriorStageEvent::PlayerDied:		return EWarriorStageState::StageFailed;
	default:									return EWarriorStageState::None;
	}
}

float UWarriorStageState_Preparing::GetDuration() const
{
	const AWarriorStageGameMode* StageGameMode = GetStageGameMode();

	return StageGameMode ? StageGameMode->GetPrepareTime() : 0.f;
}
