// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageState_WaveCleared.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

UWarriorStageState_WaveCleared::UWarriorStageState_WaveCleared()
{
	StateType = EWarriorStageState::WaveCleared;
}

void UWarriorStageState_WaveCleared::OnEnter(EWarriorStageState InPrevState)
{
	//판 통계는 이 상태로 바뀌는 알림에서 이미 이 웨이브를 클리어로 닫았다. 처치 골드는 적이 죽을 때 들어왔다
	if (AWarriorStageGameMode* StageGameMode = GetStageGameMode())
	{
		StageGameMode->UploadWaveCheckpoint();
	}
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
