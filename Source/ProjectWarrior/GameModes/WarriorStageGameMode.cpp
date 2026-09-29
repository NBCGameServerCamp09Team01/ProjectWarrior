// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageGameMode.h"
#include "WarriorStageGameState.h"
#include "ProjectWarrior/Stage/WarriorStageFlowManager.h"

AWarriorStageGameMode::AWarriorStageGameMode()
{
	GameStateClass = AWarriorStageGameState::StaticClass();
}

void AWarriorStageGameMode::SendStageEvent(EWarriorStageEvent InEvent)
{
	//P2에서 현재 상태 객체의 HandleEvent로 넘긴다. 지금은 수신 확인용 로그만 남긴다.
	UE_LOG(LogTemp, Log, TEXT("[Stage] Event %s received in state %s"),
		*UEnum::GetValueAsString(InEvent),
		*UEnum::GetValueAsString(GetCurrentStageState()));
}

EWarriorStageState AWarriorStageGameMode::GetCurrentStageState() const
{
	const AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>();

	return StageGameState ? StageGameState->GetStageState() : EWarriorStageState::None;
}

void AWarriorStageGameMode::RegisterStageFlowManager(AWarriorStageFlowManager* InFlowManager)
{
	if (!InFlowManager)
	{
		return;
	}

	if (StageFlowManager.IsValid())
	{
		if (StageFlowManager.Get() != InFlowManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Stage] %s is ignored. %s is already registered as the stage flow manager."),
				*InFlowManager->GetName(),
				*StageFlowManager->GetName());
		}
		return;
	}

	StageFlowManager = InFlowManager;
	InFlowManager->OnWaveCleared.AddUniqueDynamic(this, &ThisClass::HandleWaveCleared);

	if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetWaveInfo(0, InFlowManager->GetTotalWaveCount(), false);
	}

	UE_LOG(LogTemp, Log, TEXT("[Stage] Flow manager %s registered. Total waves: %d"),
		*InFlowManager->GetName(),
		InFlowManager->GetTotalWaveCount());
}

void AWarriorStageGameMode::HandleWaveCleared(int32 InWaveNumber)
{
	SendStageEvent(EWarriorStageEvent::AllEnemiesDead);
}
