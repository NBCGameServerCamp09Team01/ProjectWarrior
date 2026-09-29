// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageFlowManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "ProjectWarrior/Stage/WarriorStageWaveDataAsset.h"
#include "ProjectWarrior/Stage/WarriorWaveSpawner.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWarriorStageFlowManager::AWarriorStageFlowManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWarriorStageFlowManager::BeginPlay()
{
	Super::BeginPlay();

	// GameMode 등록 직후 웨이브가 시작될 수 있으므로 이벤트부터 연결한다.
	if (!bUseDebugWaves && IsValid(WaveSpawner))
	{
		WaveSpawner->OnEnemyCountChanged.AddUObject(this, &ThisClass::HandleSpawnerEnemyCountChanged);
		WaveSpawner->OnWaveCleared.AddUObject(this, &ThisClass::HandleSpawnerWaveCleared);
	}

	if (AWarriorStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<AWarriorStageGameMode>())
	{
		StageGameMode->RegisterStageFlowManager(this);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s is placed in a level without AWarriorStageGameMode. It will not run."), *GetName());
	}
}

void AWarriorStageFlowManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DebugWaveTimerHandle);
	if (IsValid(WaveSpawner))
	{
		WaveSpawner->OnEnemyCountChanged.RemoveAll(this);
		WaveSpawner->OnWaveCleared.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool AWarriorStageFlowManager::StartWave(int32 InWaveIndex)
{
	if (InWaveIndex < 0 || InWaveIndex >= GetTotalWaveCount())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] StartWave(%d) is out of range. Total waves: %d"), InWaveIndex, GetTotalWaveCount());
		return false;
	}

	ActiveWaveNumber = InWaveIndex + 1;

	if (bUseDebugWaves)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Stage][Debug] StartWave %d/%d. Clears in %.1f s"), ActiveWaveNumber, GetTotalWaveCount(), DebugWaveClearTime);

		ReportEnemyCount(DebugEnemyCount, DebugEnemyCount);
		GetWorldTimerManager().SetTimer(DebugWaveTimerHandle, this, &ThisClass::HandleDebugWaveTimerElapsed, DebugWaveClearTime, false);
		return true;
	}

	FWarriorStageWaveData WaveData;
	if (!IsValid(StageWaveData) || !IsValid(WaveSpawner) || !StageWaveData->GetWaveData(InWaveIndex, WaveData))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Wave %d has no data or spawner."), ActiveWaveNumber);
		ActiveWaveNumber = 0;
		return false;
	}

	PlannedEnemyCount = 0;
	for (const FWarriorWaveEnemySpawnData& EnemyData : WaveData.Enemies)
	{
		PlannedEnemyCount += EnemyData.Count;
	}

	if (!WaveSpawner->StartWaveFromData(WaveData))
	{
		ActiveWaveNumber = 0;
		return false;
	}

	ReportEnemyCount(PlannedEnemyCount, PlannedEnemyCount);
	return true;
}

void AWarriorStageFlowManager::StopAll()
{
	GetWorldTimerManager().ClearTimer(DebugWaveTimerHandle);
	ActiveWaveNumber = 0;

	if (IsValid(WaveSpawner))
	{
		WaveSpawner->StopSpawning();
	}
}

int32 AWarriorStageFlowManager::GetTotalWaveCount() const
{
	if (bUseDebugWaves)
	{
		return DebugWaveCount;
	}

	return IsValid(StageWaveData) ? StageWaveData->GetWaveCount() : 0;
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

	FWarriorStageWaveData WaveData;
	return IsValid(StageWaveData) && StageWaveData->GetWaveData(InWaveIndex, WaveData) && WaveData.bBossWave;
}

float AWarriorStageFlowManager::GetRestTimeOverride(int32 InWaveIndex) const
{
	if (bUseDebugWaves)
	{
		return 0.f;
	}

	FWarriorStageWaveData WaveData;
	return IsValid(StageWaveData) && StageWaveData->GetWaveData(InWaveIndex, WaveData) ? WaveData.RestTimeOverride : 0.f;
}

void AWarriorStageFlowManager::HandleSpawnerEnemyCountChanged(int32 InAliveCount, int32 InSpawnedCount)
{
	// 누적 생성 수 대신 앞으로 생성할 요청까지 포함해 남은 적 수를 보고한다.
	const int32 RemainingSpawnCount = IsValid(WaveSpawner) ? WaveSpawner->GetRemainingSpawnCount() : 0;
	ReportEnemyCount(InAliveCount + RemainingSpawnCount, PlannedEnemyCount);
}

void AWarriorStageFlowManager::HandleSpawnerWaveCleared()
{
	ReportWaveCleared(ActiveWaveNumber);
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

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Wave %d cleared"), InWaveNumber);

	OnWaveCleared.Broadcast(InWaveNumber);
}

void AWarriorStageFlowManager::HandleDebugWaveTimerElapsed()
{
	const int32 ClearedWaveNumber = ActiveWaveNumber;

	ReportEnemyCount(0, DebugEnemyCount);
	ReportWaveCleared(ClearedWaveNumber);
}
