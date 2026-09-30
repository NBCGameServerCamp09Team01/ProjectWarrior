#include "WarriorStageStatsSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/GameModes/WarriorFrontGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "WarriorProfileStatsSubsystem.h"
#include "WarriorStatTags.h"

UWarriorStageStatsSubsystem* UWarriorStageStatsSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWarriorStageStatsSubsystem>() : nullptr;
}

bool UWarriorStageStatsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 에디터·미리보기 월드에서는 만들지 않는다.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWarriorStageStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 기록은 권한 쪽에서만 한다 (D7). 싱글 플레이에서는 항상 권한 쪽이다.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	AGameModeBase* GameMode = InWorld.GetAuthGameMode();
	if (Cast<AWarriorFrontGameMode>(GameMode))
	{
		// 타이틀로 돌아오면 진행 중인 판은 중도 이탈로 끝낸다 (D9).
		if (UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(&InWorld))
		{
			if (ProfileStats->HasActiveRun())
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Front level entered with an active run. Ending it as abandoned."));
				ProfileStats->EndRun(EWarriorStatOutcome::Abandoned);
			}
		}
		return;
	}

	if (!Cast<AWarriorStageGameMode>(GameMode))
	{
		return;
	}

	AWarriorStageGameState* GameState = InWorld.GetGameState<AWarriorStageGameState>();
	if (!GameState)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] Stage level without AWarriorStageGameState. Stats will not be recorded."));
		return;
	}

	BeginStageRecord(GameState);
}

void UWarriorStageStatsSubsystem::Deinitialize()
{
	// 스테이지가 끝나기 전에 레벨을 떠났으면 중도 이탈로 마감한다.
	if (IsRecording())
	{
		FinishStageRecord(EWarriorStatOutcome::Abandoned);
	}

	if (AWarriorStageGameState* GameState = StageGameState.Get())
	{
		GameState->OnStageStateChanged.RemoveDynamic(this, &ThisClass::HandleStageStateChanged);
		GameState->OnWaveChanged.RemoveDynamic(this, &ThisClass::HandleWaveChanged);
	}
	StageGameState.Reset();

	Super::Deinitialize();
}

float UWarriorStageStatsSubsystem::GetStageTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return bRecording && World ? static_cast<float>(World->GetTimeSeconds() - StageStartWorldTime) : 0.0f;
}

int32 UWarriorStageStatsSubsystem::GetCurrentWaveNumber() const
{
	return Record.Waves.IsValidIndex(CurrentWaveIndex) ? Record.Waves[CurrentWaveIndex].WaveNumber : 0;
}

FWarriorStageRecord* UWarriorStageStatsSubsystem::GetMutableRecord()
{
	return IsRecording() ? &Record : nullptr;
}

FWarriorStatBlock* UWarriorStageStatsSubsystem::GetMutableStats()
{
	return IsRecording() ? &Record.Stats : nullptr;
}

FWarriorWaveRecord* UWarriorStageStatsSubsystem::GetMutableCurrentWave()
{
	if (!IsRecording() || !Record.Waves.IsValidIndex(CurrentWaveIndex))
	{
		return nullptr;
	}
	FWarriorWaveRecord& Wave = Record.Waves[CurrentWaveIndex];
	return Wave.bCleared ? nullptr : &Wave;
}

void UWarriorStageStatsSubsystem::BeginStageRecord(AWarriorStageGameState* InStageGameState)
{
	UWorld* World = GetWorld();
	UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(World);

	Record = FWarriorStageRecord();
	Record.RecordId = FGuid::NewGuid();
	Record.RunId = ProfileStats ? ProfileStats->BeginRunIfNeeded() : FGuid();
	Record.BuildVersion = UWarriorProfileStatsSubsystem::GetBuildVersion();
	Record.StageId = FName(*UWorld::RemovePIEPrefix(World->GetMapName()));
	Record.StartedAtUtc = FDateTime::UtcNow();

	StageStartWorldTime = World->GetTimeSeconds();
	StateEnterWorldTime = StageStartWorldTime;
	CurrentState = InStageGameState->GetStageState();
	CurrentWaveIndex = INDEX_NONE;
	bPlayerHitThisWave = false;
	bRecording = true;
	bRecordSubmitted = false;

	StageGameState = InStageGameState;
	InStageGameState->OnStageStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStageStateChanged);
	InStageGameState->OnWaveChanged.AddUniqueDynamic(this, &ThisClass::HandleWaveChanged);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Stage record started: %s (run %s)"), *Record.StageId.ToString(), *Record.RunId.ToString());
}

void UWarriorStageStatsSubsystem::FinishStageRecord(const EWarriorStatOutcome InOutcome)
{
	if (!IsRecording())
	{
		return;
	}

	AccumulateStateTime();
	if (Record.Waves.IsValidIndex(CurrentWaveIndex) && !Record.Waves[CurrentWaveIndex].bCleared)
	{
		CloseCurrentWave(false);
	}

	Record.Outcome = InOutcome;
	Record.EndedAtUtc = FDateTime::UtcNow();
	if (const AWarriorStageGameState* GameState = StageGameState.Get())
	{
		Record.TotalWaves = GameState->GetTotalWaveCount();
	}
	if (const UPlayerInventoryComponent* Inventory = FindPlayerInventory())
	{
		Record.GoldAtEnd = Inventory->GetGold();
	}

	// 먼저 표시해 두어 넘기는 도중 다시 불려도 두 번 넘기지 않게 한다.
	bRecordSubmitted = true;

	if (UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(GetWorld()))
	{
		ProfileStats->AddStageRecord(Record);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] No profile stats subsystem. Stage record %s was not stored."), *Record.StageId.ToString());
	}
}

bool UWarriorStageStatsSubsystem::IsPlayState(const EWarriorStageState InState)
{
	switch (InState)
	{
	case EWarriorStageState::Preparing:
	case EWarriorStageState::InProgress:
	case EWarriorStageState::WaveCleared:
	case EWarriorStageState::Resting:
		return true;
	default:
		return false;
	}
}

void UWarriorStageStatsSubsystem::AccumulateStateTime()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 월드 시간은 일시정지 중에 멈추므로 일시정지 시간은 자동으로 빠진다.
	const double Now = World->GetTimeSeconds();
	const double Elapsed = FMath::Max(0.0, Now - StateEnterWorldTime);
	StateEnterWorldTime = Now;

	if (Elapsed <= 0.0)
	{
		return;
	}
	if (IsPlayState(CurrentState))
	{
		Record.Stats.PlayTimeSeconds += Elapsed;
	}
	if (CurrentState == EWarriorStageState::Resting)
	{
		Record.Stats.AddExtra(WarriorStatTags::Stat_Time_Rest, Elapsed);
	}
}

void UWarriorStageStatsSubsystem::OpenWave(const int32 InWaveNumber, const bool bInBossWave)
{
	if (Record.Waves.IsValidIndex(CurrentWaveIndex) && !Record.Waves[CurrentWaveIndex].bCleared)
	{
		// 클리어 전에 다음 웨이브가 시작되면(시작 실패로 건너뛴 경우 등) 미클리어로 닫는다.
		CloseCurrentWave(false);
	}

	FWarriorWaveRecord& Wave = Record.Waves.AddDefaulted_GetRef();
	Wave.WaveNumber = InWaveNumber;
	Wave.bBossWave = bInBossWave;
	Wave.StartTimeSeconds = GetStageTimeSeconds();
	CurrentWaveIndex = Record.Waves.Num() - 1;
	bPlayerHitThisWave = false;
}

void UWarriorStageStatsSubsystem::CloseCurrentWave(const bool bInCleared)
{
	if (!Record.Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	FWarriorWaveRecord& Wave = Record.Waves[CurrentWaveIndex];
	if (Wave.bCleared)
	{
		return;
	}

	Wave.bCleared = bInCleared;
	if (bInCleared)
	{
		Wave.ClearTimeSeconds = GetStageTimeSeconds();
		++Record.WavesCleared;
		if (!bPlayerHitThisWave)
		{
			Record.Stats.AddExtra(WarriorStatTags::Stat_Defense_Wave_NoHit, 1.0);
		}
	}
}

UPlayerInventoryComponent* UWarriorStageStatsSubsystem::FindPlayerInventory() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const AWarriorPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AWarriorPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetPlayerInventoryComponent() : nullptr;
}

void UWarriorStageStatsSubsystem::HandleStageStateChanged(const EWarriorStageState NewState, const EWarriorStageState OldState)
{
	if (!IsRecording())
	{
		return;
	}

	AccumulateStateTime();
	CurrentState = NewState;

	switch (NewState)
	{
	case EWarriorStageState::WaveCleared:
		CloseCurrentWave(true);
		break;
	case EWarriorStageState::StageCleared:
		// 마지막 웨이브는 WaveCleared를 거치지 않고 바로 StageCleared로 갈 수 있다.
		CloseCurrentWave(true);
		FinishStageRecord(EWarriorStatOutcome::Cleared);
		break;
	case EWarriorStageState::StageFailed:
		FinishStageRecord(EWarriorStatOutcome::Failed);
		break;
	default:
		break;
	}
}

void UWarriorStageStatsSubsystem::HandleWaveChanged(const int32 WaveNumber, const int32 TotalWaveCount, const bool bBossWave)
{
	if (!IsRecording())
	{
		return;
	}

	Record.TotalWaves = TotalWaveCount;

	// GameMode는 등록 직후 0번(웨이브 전)으로 한 번 방송한다. 같은 번호가 다시 와도 새로 열지 않는다.
	if (WaveNumber <= 0 || WaveNumber == GetCurrentWaveNumber())
	{
		return;
	}

	OpenWave(WaveNumber, bBossWave);
}
