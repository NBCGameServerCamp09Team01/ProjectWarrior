#include "WarriorProfileStatsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "ProjectWarrior/ProjectWarrior.h"

namespace WarriorProfileStats_Private
{
	const TCHAR* OutcomeToString(const EWarriorStatOutcome InOutcome)
	{
		switch (InOutcome)
		{
		case EWarriorStatOutcome::InProgress: return TEXT("InProgress");
		case EWarriorStatOutcome::Cleared:    return TEXT("Cleared");
		case EWarriorStatOutcome::Failed:     return TEXT("Failed");
		case EWarriorStatOutcome::Abandoned:  return TEXT("Abandoned");
		}
		return TEXT("Unknown");
	}

	template <typename RecordType>
	void TrimOldest(TArray<RecordType>& Records, const int32 MaxCount)
	{
		const int32 Overflow = Records.Num() - MaxCount;
		if (Overflow > 0)
		{
			Records.RemoveAt(0, Overflow);
		}
	}
}

UWarriorProfileStatsSubsystem* UWarriorProfileStatsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWarriorProfileStatsSubsystem>() : nullptr;
}

FString UWarriorProfileStatsSubsystem::GetBuildVersion()
{
	// EngineSettings 모듈 의존성을 늘리지 않도록 설정 파일에서 직접 읽는다.
	FString Version;
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	}
	return Version;
}

void UWarriorProfileStatsSubsystem::Deinitialize()
{
	// 게임(또는 PIE)을 끌 때 진행 중인 판은 중도 이탈로 마감한다.
	EndRun(EWarriorStatOutcome::Abandoned);
	Super::Deinitialize();
}

FGuid UWarriorProfileStatsSubsystem::BeginRunIfNeeded()
{
	if (CurrentRun.IsActive())
	{
		return CurrentRun.RunId;
	}

	CurrentRun = FWarriorRunRecord();
	CurrentRun.RunId = FGuid::NewGuid();
	CurrentRun.BuildVersion = GetBuildVersion();
	CurrentRun.StartedAtUtc = FDateTime::UtcNow();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Run started %s (build '%s')"), *CurrentRun.RunId.ToString(), *CurrentRun.BuildVersion);
	return CurrentRun.RunId;
}

void UWarriorProfileStatsSubsystem::EndRun(const EWarriorStatOutcome InOutcome)
{
	if (!CurrentRun.IsActive())
	{
		return;
	}
	if (InOutcome == EWarriorStatOutcome::InProgress)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] EndRun ignored: outcome must not be InProgress."));
		return;
	}

	CurrentRun.Outcome = InOutcome;
	CurrentRun.EndedAtUtc = FDateTime::UtcNow();

	++Lifetime.RunsPlayed;
	if (InOutcome == EWarriorStatOutcome::Cleared)
	{
		++Lifetime.RunsCleared;
	}

	RunRecords.Add(CurrentRun);
	WarriorProfileStats_Private::TrimOldest(RunRecords, MaxRunRecords);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Run ended %s: %s, stages %d (cleared %d), kills %d, gold earned %d"),
		*CurrentRun.RunId.ToString(), WarriorProfileStats_Private::OutcomeToString(InOutcome),
		CurrentRun.StagesPlayed, CurrentRun.StagesCleared, CurrentRun.Stats.Attack.Kills, CurrentRun.Stats.Economy.GoldEarned);

	// 방송 중에 새 판이 시작되어도 덮어쓰지 않도록 복사본을 넘기고 먼저 비운다.
	const FWarriorRunRecord EndedRun = CurrentRun;
	CurrentRun = FWarriorRunRecord();
	OnRunEnded.Broadcast(EndedRun);
}

void UWarriorProfileStatsSubsystem::AddStageRecord(const FWarriorStageRecord& InStageRecord)
{
	if (InStageRecord.Outcome == EWarriorStatOutcome::InProgress)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] AddStageRecord ignored: stage %s is still in progress."), *InStageRecord.StageId.ToString());
		return;
	}

	const FGuid RunId = BeginRunIfNeeded();
	FWarriorStageRecord StageRecord = InStageRecord;
	if (StageRecord.RunId != RunId)
	{
		if (StageRecord.RunId.IsValid())
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] Stage %s belonged to run %s, which is no longer active. Attaching to run %s."),
				*StageRecord.StageId.ToString(), *StageRecord.RunId.ToString(), *RunId.ToString());
		}
		StageRecord.RunId = RunId;
	}
	if (!StageRecord.RecordId.IsValid())
	{
		StageRecord.RecordId = FGuid::NewGuid();
	}

	// 판: 결과와 상관없이 이 판에서 한 스테이지 전부를 합산한다.
	CurrentRun.Stats.Merge(StageRecord.Stats);
	++CurrentRun.StagesPlayed;
	if (StageRecord.Outcome == EWarriorStatOutcome::Cleared)
	{
		++CurrentRun.StagesCleared;
	}
	CurrentRun.StageRecordIds.Add(StageRecord.RecordId);

	// 누적
	ApplyStageToLifetime(StageRecord);

	StageRecords.Add(StageRecord);
	WarriorProfileStats_Private::TrimOldest(StageRecords, MaxStageRecords);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Stage recorded %s: %s, waves %d/%d, kills %d, gold earned %d, play %.1f s"),
		*StageRecord.StageId.ToString(), WarriorProfileStats_Private::OutcomeToString(StageRecord.Outcome),
		StageRecord.WavesCleared, StageRecord.TotalWaves, StageRecord.Stats.Attack.Kills,
		StageRecord.Stats.Economy.GoldEarned, StageRecord.Stats.PlayTimeSeconds);

	OnStageRecorded.Broadcast(StageRecord);

	// D9: 게임 흐름이 정해지기 전 임시 규칙
	switch (StageRecord.Outcome)
	{
	case EWarriorStatOutcome::Failed:
		EndRun(EWarriorStatOutcome::Failed);
		break;
	case EWarriorStatOutcome::Abandoned:
		EndRun(EWarriorStatOutcome::Abandoned);
		break;
	case EWarriorStatOutcome::Cleared:
		if (bEndRunOnStageCleared)
		{
			EndRun(EWarriorStatOutcome::Cleared);
		}
		break;
	default:
		break;
	}
}

void UWarriorProfileStatsSubsystem::ApplyStageToLifetime(const FWarriorStageRecord& InStageRecord)
{
	++Lifetime.StagesPlayed;

	// D5: 중도 이탈은 플레이 수만 반영한다.
	if (InStageRecord.Outcome == EWarriorStatOutcome::Abandoned)
	{
		return;
	}

	Lifetime.Stats.Merge(InStageRecord.Stats);

	if (InStageRecord.Outcome == EWarriorStatOutcome::Cleared)
	{
		++Lifetime.StagesCleared;
		const float ClearTime = static_cast<float>(InStageRecord.Stats.PlayTimeSeconds);
		if (ClearTime > 0.0f && (Lifetime.BestStageClearTimeSeconds <= 0.0f || ClearTime < Lifetime.BestStageClearTimeSeconds))
		{
			Lifetime.BestStageClearTimeSeconds = ClearTime;
		}
	}
	else if (InStageRecord.Outcome == EWarriorStatOutcome::Failed)
	{
		++Lifetime.StagesFailed;
	}

	for (const FWarriorWaveRecord& Wave : InStageRecord.Waves)
	{
		Lifetime.MaxWaveReached = FMath::Max(Lifetime.MaxWaveReached, Wave.WaveNumber);
	}

	for (const FWarriorEnemyTypeStats& Enemy : InStageRecord.Enemies)
	{
		FWarriorEnemyTypeStats& LifetimeEnemy = Lifetime.EnemiesByType.FindOrAdd(Enemy.EnemyType);
		LifetimeEnemy.Merge(Enemy);
	}
}

bool UWarriorProfileStatsSubsystem::GetLastStageRecord(FWarriorStageRecord& OutStageRecord) const
{
	if (StageRecords.IsEmpty())
	{
		return false;
	}
	OutStageRecord = StageRecords.Last();
	return true;
}

bool UWarriorProfileStatsSubsystem::GetLastRunRecord(FWarriorRunRecord& OutRunRecord) const
{
	if (RunRecords.IsEmpty())
	{
		return false;
	}
	OutRunRecord = RunRecords.Last();
	return true;
}

void UWarriorProfileStatsSubsystem::ResetAll()
{
	CurrentRun = FWarriorRunRecord();
	RunRecords.Reset();
	StageRecords.Reset();
	Lifetime = FWarriorLifetimeStats();
}
