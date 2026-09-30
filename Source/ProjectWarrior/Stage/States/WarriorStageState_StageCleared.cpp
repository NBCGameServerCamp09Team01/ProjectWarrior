// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_StageCleared.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_StageCleared::UWarriorStageState_StageCleared()
{
	StateType = EWarriorStageState::StageCleared;
}

void UWarriorStageState_StageCleared::OnEnter(EWarriorStageState InPrevState)
{
	if (AWarriorStageGameMode* StageGameMode = GetStageGameMode())
	{
		StageGameMode->StopAllWaves();
		StageGameMode->ResetStageUpgrades();
	}
}
