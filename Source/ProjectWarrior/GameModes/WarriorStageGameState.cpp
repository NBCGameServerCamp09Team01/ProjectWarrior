// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

AWarriorStageGameState::AWarriorStageGameState()
{
	//기본 권한 표. 적지 않은 상태(None, Preparing, InProgress, WaveCleared)는 FWarriorStagePermission 기본값
	//(이동·전투·상호작용 허용, 상점 불가)을 쓴다.
	FWarriorStagePermission Locked;
	Locked.bCanMove = false;
	Locked.bCanCombat = false;
	Locked.bCanInteract = false;
	Locked.bCanShop = false;
	StatePermissions.Add(EWarriorStageState::Initializing, Locked);

	FWarriorStagePermission Resting;
	Resting.bCanShop = true;
	StatePermissions.Add(EWarriorStageState::Resting, Resting);

	FWarriorStagePermission Result = Locked;
	Result.bUIInputMode = true;
	StatePermissions.Add(EWarriorStageState::StageCleared, Result);
	StatePermissions.Add(EWarriorStageState::StageFailed, Result);
}

float AWarriorStageGameState::GetStateRemainingTime() const
{
	if (StateEndTime <= 0.f)
	{
		return 0.f;
	}

	return FMath::Max(0.f, StateEndTime - GetServerWorldTimeSeconds());
}

FWarriorStagePermission AWarriorStageGameState::GetPermission(EWarriorStageState InState) const
{
	if (const FWarriorStagePermission* FoundPermission = StatePermissions.Find(InState))
	{
		return *FoundPermission;
	}

	return FWarriorStagePermission();
}

bool AWarriorStageGameState::IsActionAllowedInCurrentState(EWarriorStageAction InAction) const
{
	return GetPermission(StageState).IsAllowed(InAction);
}

bool AWarriorStageGameState::IsActionAllowed(const UObject* WorldContextObject, EWarriorStageAction InAction)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return true;
	}

	const AWarriorStageGameState* StageGameState = World->GetGameState<AWarriorStageGameState>();
	if (!StageGameState)
	{
		return true;
	}

	return StageGameState->IsActionAllowedInCurrentState(InAction);
}

void AWarriorStageGameState::SetStageState(EWarriorStageState InNewState, float InDuration)
{
	if (StageState == InNewState)
	{
		return;
	}

	const EWarriorStageState OldState = StageState;
	StageState = InNewState;
	StateEndTime = InDuration > 0.f ? GetServerWorldTimeSeconds() + InDuration : 0.f;

	UE_LOG(LogTemp, Log, TEXT("[Stage] State %s -> %s (Duration %.1f)"),
		*UEnum::GetValueAsString(OldState),
		*UEnum::GetValueAsString(InNewState),
		InDuration);

	OnStageStateChanged.Broadcast(InNewState, OldState);
}

void AWarriorStageGameState::SetWaveInfo(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave)
{
	WaveNumber = FMath::Max(0, InWaveNumber);
	TotalWaveCount = FMath::Max(0, InTotalWaveCount);
	bBossWave = bInBossWave;

	OnWaveChanged.Broadcast(WaveNumber, TotalWaveCount, bBossWave);
}

void AWarriorStageGameState::SetEnemyCount(int32 InAliveCount, int32 InTotalCount)
{
	if (AliveEnemyCount == InAliveCount && TotalEnemyCount == InTotalCount)
	{
		return;
	}

	AliveEnemyCount = FMath::Max(0, InAliveCount);
	TotalEnemyCount = FMath::Max(0, InTotalCount);

	OnEnemyCountChanged.Broadcast(AliveEnemyCount, TotalEnemyCount);
}
