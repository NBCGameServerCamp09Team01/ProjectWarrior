// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageFlowManager.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWarriorStageFlowManager::AWarriorStageFlowManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWarriorStageFlowManager::BeginPlay()
{
	Super::BeginPlay();

	if (AWarriorStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<AWarriorStageGameMode>())
	{
		StageGameMode->RegisterStageFlowManager(this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage] %s is placed in a level without AWarriorStageGameMode. It will not run."), *GetName());
	}
}

void AWarriorStageFlowManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DebugWaveTimerHandle);

	Super::EndPlay(EndPlayReason);
}

bool AWarriorStageFlowManager::StartWave(int32 InWaveIndex)
{
	if (InWaveIndex < 0 || InWaveIndex >= GetTotalWaveCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage] StartWave(%d) is out of range. Total waves: %d"), InWaveIndex, GetTotalWaveCount());
		return false;
	}

	ActiveWaveNumber = InWaveIndex + 1;

	if (bUseDebugWaves)
	{
		UE_LOG(LogTemp, Log, TEXT("[Stage][Debug] StartWave %d/%d. Clears in %.1f s"), ActiveWaveNumber, GetTotalWaveCount(), DebugWaveClearTime);

		ReportEnemyCount(DebugEnemyCount, DebugEnemyCount);
		GetWorldTimerManager().SetTimer(DebugWaveTimerHandle, this, &ThisClass::HandleDebugWaveTimerElapsed, DebugWaveClearTime, false);
		return true;
	}

	// ↓ 웨이브 담당 구현
	// FWarriorStageWaveData WaveData;
	// if (!StageWaveData || !StageWaveData->GetWaveData(InWaveIndex, WaveData) || !WaveSpawner) { return false; }
	// WaveSpawner->StartWaveFromData(WaveData);
	// return true;
	return false;
}

void AWarriorStageFlowManager::StopAll()
{
	GetWorldTimerManager().ClearTimer(DebugWaveTimerHandle);
	ActiveWaveNumber = 0;

	// ↓ 웨이브 담당 구현: WaveSpawner->StopSpawning();
}

int32 AWarriorStageFlowManager::GetTotalWaveCount() const
{
	if (bUseDebugWaves)
	{
		return DebugWaveCount;
	}

	// ↓ 웨이브 담당 구현: return StageWaveData ? StageWaveData->GetWaveCount() : 0;
	return 0;
}

bool AWarriorStageFlowManager::IsLastWave(int32 InWaveIndex) const
{
	return InWaveIndex >= GetTotalWaveCount() - 1;
}

bool AWarriorStageFlowManager::IsBossWave(int32 InWaveIndex) const
{
	if (bUseDebugWaves)
	{
		return InWaveIndex == DebugWaveCount - 1;
	}

	// ↓ 웨이브 담당 구현: 웨이브 데이터의 bBossWave
	return false;
}

float AWarriorStageFlowManager::GetRestTimeOverride(int32 InWaveIndex) const
{
	if (bUseDebugWaves)
	{
		return 0.f;
	}

	// ↓ 웨이브 담당 구현: 웨이브 데이터의 RestTimeOverride
	return 0.f;
}

void AWarriorStageFlowManager::ReportEnemyCount(int32 InAliveCount, int32 InTotalCount)
{
	if (AWarriorStageGameState* StageGameState = GetWorld()->GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetEnemyCount(InAliveCount, InTotalCount);
	}
}

void AWarriorStageFlowManager::ReportWaveCleared(int32 InWaveNumber)
{
	if (InWaveNumber <= 0 || InWaveNumber == LastClearedWaveNumber)
	{
		return;
	}

	LastClearedWaveNumber = InWaveNumber;
	ActiveWaveNumber = 0;

	UE_LOG(LogTemp, Log, TEXT("[Stage] Wave %d cleared"), InWaveNumber);

	OnWaveCleared.Broadcast(InWaveNumber);
}

void AWarriorStageFlowManager::HandleDebugWaveTimerElapsed()
{
	const int32 ClearedWaveNumber = ActiveWaveNumber;

	ReportEnemyCount(0, DebugEnemyCount);
	ReportWaveCleared(ClearedWaveNumber);
}
