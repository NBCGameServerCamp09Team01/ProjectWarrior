// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_Initializing.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_Initializing::UWarriorStageState_Initializing()
{
	StateType = EWarriorStageState::Initializing;
}

EWarriorStageState UWarriorStageState_Initializing::HandleEvent(EWarriorStageEvent InEvent)
{
	switch (InEvent)
	{
	case EWarriorStageEvent::StageReady:	return EWarriorStageState::Preparing;
	default:								return EWarriorStageState::None;
	}
}
