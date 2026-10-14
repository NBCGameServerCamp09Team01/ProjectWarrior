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
	case EWarriorStageEvent::StageReady:
	{
		//체크포인트에서 이어 가면 준비 시간 대신 다음 웨이브의 쉬는 시간부터(M22)
		const AWarriorStageGameMode* StageGameMode = GetStageGameMode();
		return StageGameMode && StageGameMode->IsResumingFromCheckpoint() ? EWarriorStageState::Resting : EWarriorStageState::Preparing;
	}
	default:								return EWarriorStageState::None;
	}
}
