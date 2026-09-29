// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_WaveCleared.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_WaveCleared::UWarriorStageState_WaveCleared()
{
	StateType = EWarriorStageState::WaveCleared;
}

EWarriorStageState UWarriorStageState_WaveCleared::HandleEvent(EWarriorStageEvent InEvent)
{
	switch (InEvent)
	{
	case EWarriorStageEvent::StateTimerElapsed:
	{
		const AWarriorStageGameMode* StageGameMode = GetStageGameMode();
		const bool bLastWave = !StageGameMode || StageGameMode->IsCurrentWaveLast();

		return bLastWave ? EWarriorStageState::StageCleared : EWarriorStageState::Resting;
	}
	case EWarriorStageEvent::PlayerDied:	return EWarriorStageState::StageFailed;
	default:								return EWarriorStageState::None;
	}
}

float UWarriorStageState_WaveCleared::GetDuration() const
{
	const AWarriorStageGameMode* StageGameMode = GetStageGameMode();

	return StageGameMode ? StageGameMode->GetWaveClearedTime() : 0.f;
}
