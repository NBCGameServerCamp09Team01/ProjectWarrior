// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_StageFailed.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_StageFailed::UWarriorStageState_StageFailed()
{
	StateType = EWarriorStageState::StageFailed;
}

void UWarriorStageState_StageFailed::OnEnter(EWarriorStageState InPrevState)
{
	if (AWarriorStageGameMode* StageGameMode = GetStageGameMode())
	{
		StageGameMode->StopAllWaves();
		StageGameMode->ResetStageUpgrades();
	}
}
