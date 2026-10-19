// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_InProgress.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_InProgress::UWarriorStageState_InProgress()
{
	StateType = EWarriorStageState::InProgress;
}

void UWarriorStageState_InProgress::OnEnter(EWarriorStageState InPrevState)
{
	if (AWarriorStageGameMode* StageGameMode = GetStageGameMode())
	{
		StageGameMode->StartNextWave();
	}
}

EWarriorStageState UWarriorStageState_InProgress::HandleEvent(EWarriorStageEvent InEvent)
{
	switch (InEvent)
	{
	case EWarriorStageEvent::AllEnemiesDead:	return EWarriorStageState::WaveCleared;
	case EWarriorStageEvent::PlayerDied:		return EWarriorStageState::StageFailed;
	default:									return EWarriorStageState::None;
	}
}
