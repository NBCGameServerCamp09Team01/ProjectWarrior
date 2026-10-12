// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageGameMode.h"
#include "WarriorStageGameState.h"
#include "ProjectWarrior/Stage/WarriorStageFlowManager.h"
#include "ProjectWarrior/Stage/WarriorStageWaveDataAsset.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_Initializing.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_Preparing.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_InProgress.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_WaveCleared.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_Resting.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_StageCleared.h"
#include "ProjectWarrior/Stage/States/WarriorStageState_StageFailed.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "ProjectWarrior/Components/Upgrade/StageUpgradeComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"
#include "TimerManager.h"



AWarriorStageGameMode::AWarriorStageGameMode()
{
	GameStateClass = AWarriorStageGameState::StaticClass();
}

void AWarriorStageGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ResolveStageLaunch();
}

void AWarriorStageGameMode::BeginPlay()
{
	Super::BeginPlay();

	CreateStates();
	ChangeState(EWarriorStageState::Initializing);
	TryFinishInitialize();
}

void AWarriorStageGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StateTimerHandle);
	PendingEvents.Reset();

	Super::EndPlay(EndPlayReason);
}

void AWarriorStageGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	TryFinishInitialize();
}

void AWarriorStageGameMode::SendStageEvent(EWarriorStageEvent InEvent)
{
	if (bChangingState)
	{
		PendingEvents.Add(InEvent);
		return;
	}

	if (!CurrentState)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Event %s ignored. State machine is not started."), *UEnum::GetValueAsString(InEvent));
		return;
	}

	const EWarriorStageState NextState = CurrentState->HandleEvent(InEvent);
	if (NextState == EWarriorStageState::None)
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Stage] Event %s ignored in state %s"),
			*UEnum::GetValueAsString(InEvent),
			*UEnum::GetValueAsString(CurrentState->GetStateType()));
		return;
	}

	ChangeState(NextState);
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
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s is ignored. %s is already registered as the stage flow manager."),
				*InFlowManager->GetName(),
				*StageFlowManager->GetName());
		}
		return;
	}

	StageFlowManager = InFlowManager;
	InFlowManager->OnWaveCleared.AddUniqueDynamic(this, &ThisClass::HandleWaveCleared);

	//웨이브 수를 GameState에 알리기 전에 이 스테이지의 웨이브로 바꾼다.
	ApplyStageWaveData(InFlowManager);

	if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetWaveInfo(0, InFlowManager->GetTotalWaveCount(), false);
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Flow manager %s registered. Total waves: %d"),
		*InFlowManager->GetName(),
		InFlowManager->GetTotalWaveCount());

	TryFinishInitialize();
}

void AWarriorStageGameMode::StartNextWave()
{
	++CurrentWaveIndex;

	AWarriorStageFlowManager* FlowManager = GetStageFlowManager();
	const int32 TotalWaveCount = FlowManager ? FlowManager->GetTotalWaveCount() : 0;
	const bool bBossWave = FlowManager ? FlowManager->IsBossWave(CurrentWaveIndex) : false;

	if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetWaveInfo(CurrentWaveIndex + 1, TotalWaveCount, bBossWave);
	}

	if (!FlowManager || !FlowManager->StartWave(CurrentWaveIndex))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Wave %d could not start. Skipping it."), CurrentWaveIndex + 1);

		//InProgress의 OnEnter 안(상태 변경 중)이라 큐에 들어가고, 변경이 끝난 뒤 WaveCleared로 넘어간다.
		SendStageEvent(EWarriorStageEvent::AllEnemiesDead);
	}
}

bool AWarriorStageGameMode::IsCurrentWaveLast() const
{
	const AWarriorStageFlowManager* FlowManager = GetStageFlowManager();

	return !FlowManager || FlowManager->IsLastWave(CurrentWaveIndex);
}

float AWarriorStageGameMode::GetNextRestTime() const
{
	const AWarriorStageFlowManager* FlowManager = GetStageFlowManager();
	const float OverrideTime = FlowManager ? FlowManager->GetRestTimeOverride(CurrentWaveIndex + 1) : 0.f;

	return OverrideTime > 0.f ? OverrideTime : RestTime;
}

void AWarriorStageGameMode::StopAllWaves()
{
	if (AWarriorStageFlowManager* FlowManager = GetStageFlowManager())
	{
		FlowManager->StopAll();
	}
}

void AWarriorStageGameMode::HandleWaveCleared(int32 InWaveNumber)
{
	SendStageEvent(EWarriorStageEvent::AllEnemiesDead);
}

void AWarriorStageGameMode::CreateStates()
{
	States.Reset();

	const TArray<UWarriorStageStateBase*> NewStates =
	{
		NewObject<UWarriorStageState_Initializing>(this),
		NewObject<UWarriorStageState_Preparing>(this),
		NewObject<UWarriorStageState_InProgress>(this),
		NewObject<UWarriorStageState_WaveCleared>(this),
		NewObject<UWarriorStageState_Resting>(this),
		NewObject<UWarriorStageState_StageCleared>(this),
		NewObject<UWarriorStageState_StageFailed>(this)
	};

	//상태 객체는 Outer(this)로 GameMode를 얻으므로 별도 초기화가 필요 없다.
	for (UWarriorStageStateBase* NewState : NewStates)
	{
		States.Add(NewState->GetStateType(), NewState);
	}
}

void AWarriorStageGameMode::ChangeState(EWarriorStageState InNewState)
{
	UWarriorStageStateBase* NextState = States.FindRef(InNewState);
	if (!NextState)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] No state object for %s"), *UEnum::GetValueAsString(InNewState));
		return;
	}

	bChangingState = true;
	GetWorldTimerManager().ClearTimer(StateTimerHandle);

	const EWarriorStageState PrevState = CurrentState ? CurrentState->GetStateType() : EWarriorStageState::None;
	if (CurrentState)
	{
		CurrentState->OnExit(InNewState);
	}

	CurrentState = NextState;

	const bool bUsesTimer = CurrentState->UsesTimer();
	const float Duration = bUsesTimer ? CurrentState->GetDuration() : 0.f;

	//결과 화면이 상태 변화 알림을 받을 때 결과가 이미 있어야 하므로, 끝 상태는 상태를 바꾸기 전에 결과부터 확정한다.
	if (InNewState == EWarriorStageState::StageCleared || InNewState == EWarriorStageState::StageFailed)
	{
		FinishRun(InNewState == EWarriorStageState::StageCleared);
	}

	if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetStageState(InNewState, Duration);
	}

	CurrentState->OnEnter(PrevState);

	if (bUsesTimer)
	{
		if (Duration > 0.f)
		{
			GetWorldTimerManager().SetTimer(StateTimerHandle, this, &ThisClass::HandleStateTimerElapsed, Duration, false);
		}
		else
		{
			//길이가 0 이하면 멈추지 않도록 바로 다음 상태로 넘긴다.
			PendingEvents.Add(EWarriorStageEvent::StateTimerElapsed);
		}
	}

	bChangingState = false;
	ProcessPendingEvents();
}

void AWarriorStageGameMode::TryFinishInitialize()
{
	if (!CurrentState || CurrentState->GetStateType() != EWarriorStageState::Initializing)
	{
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const bool bPlayerSpawned = PlayerController && PlayerController->GetPawn();

	if (!StageFlowManager.IsValid() || !bPlayerSpawned)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Waiting for initialize. FlowManager: %s, Player pawn: %s"),
			StageFlowManager.IsValid() ? TEXT("ready") : TEXT("missing"),
			bPlayerSpawned ? TEXT("ready") : TEXT("missing"));
		return;
	}

	BeginRun();
	SendStageEvent(EWarriorStageEvent::StageReady);
}

void AWarriorStageGameMode::HandleStateTimerElapsed()
{
	SendStageEvent(EWarriorStageEvent::StateTimerElapsed);
}

void AWarriorStageGameMode::ProcessPendingEvents()
{
	while (!bChangingState && PendingEvents.Num() > 0)
	{
		const EWarriorStageEvent PendingEvent = PendingEvents[0];
		PendingEvents.RemoveAt(0);

		SendStageEvent(PendingEvent);
	}
}


void AWarriorStageGameMode::ResetStageUpgrades()
{
	if (!GameState)
	{
		return;
	}
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const AWarriorPlayerState* WarriorPS = Cast<AWarriorPlayerState>(PS))
		{
			if (UStageUpgradeComponent* UpgradeComp = WarriorPS->GetStageUpgradeComponent())
			{
				UpgradeComp->ResetAll();
			}
		}
	}
}

void AWarriorStageGameMode::BeginRun()
{
	//지금은 여기서 RunId를 만든다. GameInstance가 들어오면 그쪽의 한 판 시작 함수에서 발급받고,
	//서버 연동 뒤에는 서버가 발급한 값을 쓴다. 호출 위치는 그대로다.
	RunId = FGuid::NewGuid();
	RunStartRealTime = GetWorld()->GetRealTimeSeconds();
	bRunFinished = false;

	if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetRunId(RunId);
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Run started. RunId %s, Stage %s, Difficulty %d"),
		*RunId.ToString(EGuidFormats::DigitsWithHyphens),
		*GetResolvedStageId().ToString(),
		Difficulty);

	//서버에 스테이지 플레이 시작(응답을 기다리지 않는다). 받은 stagePlayId로 FinishRun에서 결과를 낸다
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this)) { StagePlay->StartStagePlay(GetResolvedStageId()); }
}

void AWarriorStageGameMode::FinishRun(bool bInCleared)
{
	if (bRunFinished)
	{
		return;
	}
	bRunFinished = true;

	//EarnedGold·KillCount는 결과 통계 쪽에서 채운다.
	FWarriorStageResult Result;
	Result.RunId = RunId;
	Result.StageId = GetResolvedStageId();
	Result.Difficulty = Difficulty;
	Result.bCleared = bInCleared;
	Result.PlayTimeSeconds = static_cast<float>(GetWorld()->GetRealTimeSeconds() - RunStartRealTime);

	AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>();
	if (StageGameState)
	{
		Result.ReachedWave = StageGameState->GetWaveNumber();
		Result.TotalWaveCount = StageGameState->GetTotalWaveCount();
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Run finished. RunId %s, %s, Wave %d/%d, PlayTime %.1f s"),
		*Result.RunId.ToString(EGuidFormats::DigitsWithHyphens),
		Result.bCleared ? TEXT("Cleared") : TEXT("Failed"),
		Result.ReachedWave,
		Result.TotalWaveCount,
		Result.PlayTimeSeconds);

	//서버에 결과 제출(검사·보상·저장은 서버가 한다)
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this)) { StagePlay->SubmitResult(Result); }

	//GameInstance 연결 지점: 결과 기록과 보상 계산은 여기서 같은 Result를 넘기면 된다.
	if (StageGameState)
	{
		StageGameState->SetStageResult(Result);
	}
}

void AWarriorStageGameMode::ResolveStageLaunch()
{
	//PIE에서는 패키지 이름에 UEDPIE 접두사가 붙으므로 떼고 목록과 비교한다.
	const FString LevelPackageName = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());

	const UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this);
	const TCHAR* Source = nullptr;

	if (LaunchSubsystem && LaunchSubsystem->HasLaunchInfo())
	{
		ResolvedLaunchInfo = LaunchSubsystem->GetLaunchInfo();
		Source = TEXT("selection");
	}
	else if (!StageId.IsNone())
	{
		UWarriorStageLaunchSubsystem::BuildLaunchInfo(StageId, ResolvedLaunchInfo);
		Source = TEXT("GameMode StageId");
	}
	else if (UWarriorStageLaunchSubsystem::BuildLaunchInfoForLevel(LevelPackageName, ResolvedLaunchInfo))
	{
		Source = TEXT("catalog level");
	}
	else
	{
		ResolvedLaunchInfo = FWarriorStageLaunchInfo();
		ResolvedLaunchInfo.StageId = FName(*UGameplayStatics::GetCurrentLevelName(this));

		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] No stage key for level %s. Using the level name as the stage id; the server will reject it."), *LevelPackageName);
		return;
	}

	if (!ResolvedLaunchInfo.bFromCatalog)
	{
		//임시: 목록에 없는 키도 그대로 쓴다. 게임 파일 검사 단계가 생기면 거기서 막는다. 인계: stage-p4-handover-1009.md
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Stage %s is not in the stage catalog. The level's own waves will be used."), *ResolvedLaunchInfo.StageId.ToString());
	}
	else if (!ResolvedLaunchInfo.Level.IsNull() && ResolvedLaunchInfo.Level.ToSoftObjectPath().GetLongPackageName() != LevelPackageName)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Stage %s is listed for level %s, but %s is open."),
			*ResolvedLaunchInfo.StageId.ToString(),
			*ResolvedLaunchInfo.Level.ToSoftObjectPath().GetLongPackageName(),
			*LevelPackageName);
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Stage %s resolved from %s. Waves: %s"),
		*ResolvedLaunchInfo.StageId.ToString(),
		Source,
		ResolvedLaunchInfo.WaveData.IsNull() ? TEXT("level") : *ResolvedLaunchInfo.WaveData.ToSoftObjectPath().GetAssetName());
}

void AWarriorStageGameMode::ApplyStageWaveData(AWarriorStageFlowManager* InFlowManager) const
{
	if (!InFlowManager || ResolvedLaunchInfo.WaveData.IsNull())
	{
		return;
	}

	UWarriorStageWaveDataAsset* WaveData = ResolvedLaunchInfo.WaveData.LoadSynchronous();
	if (!WaveData)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Wave data %s for stage %s could not be loaded. The level's own waves will be used."),
			*ResolvedLaunchInfo.WaveData.ToString(),
			*ResolvedLaunchInfo.StageId.ToString());
		return;
	}

	InFlowManager->SetStageWaveData(WaveData);
}

