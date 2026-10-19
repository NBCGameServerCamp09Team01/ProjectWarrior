// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_Resting.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_Resting::UWarriorStageState_Resting()
{
	StateType = EWarriorStageState::Resting;
}

EWarriorStageState UWarriorStageState_Resting::HandleEvent(EWarriorStageEvent InEvent)
{
	switch (InEvent)
	{
	case EWarriorStageEvent::StateTimerElapsed:
	case EWarriorStageEvent::SkipRest:		return EWarriorStageState::InProgress;
	case EWarriorStageEvent::PlayerDied:	return EWarriorStageState::StageFailed;
	default:								return EWarriorStageState::None;
	}
}

float UWarriorStageState_Resting::GetDuration() const
{
	const AWarriorStageGameMode* StageGameMode = GetStageGameMode();

	return StageGameMode ? StageGameMode->GetNextRestTime() : 0.f;
}
