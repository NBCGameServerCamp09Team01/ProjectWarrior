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
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"
#include "ProjectWarrior/Controllers/WarriorStagePlayerController.h"
#include "ProjectWarrior/Stage/WarriorStageSettings.h"
#include "ProjectWarrior/Stage/WarriorPlayCheckpoint.h"
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

	//M24: 이어하기면 StartStagePlay 안에서 바로 구성이 오므로 BeginRun(TryFinishInitialize) 전에 구독한다
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this))
	{
		WavePlanChangedHandle = StagePlay->OnWavePlanChanged.AddUObject(this, &ThisClass::HandleWavePlanChanged);
	}

	CreateStates();
	ChangeState(EWarriorStageState::Initializing);
	TryFinishInitialize();
}

void AWarriorStageGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StateTimerHandle);
	GetWorldTimerManager().ClearTimer(WavePlanWaitHandle);
	PendingEvents.Reset();

	//서브시스템은 레벨보다 오래 살므로 사라지는 GameMode를 알림 대상에서 뺀다
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this))
	{
		StagePlay->OnWavePlanChanged.Remove(WavePlanChangedHandle);
	}

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

	//M24: 여기서는 웨이브를 넣지 않는다. 서버 플레이가 있는 판은 판 시작 응답의 구성(ApplyServerWavePlan),
	//서버 플레이가 없는 개발 판만 로컬 DA(ApplyStageWaveData)를 HandleWavePlanChanged에서 넣는다.
	//그래서 등록 때의 총 웨이브 수는 0이고, 구성이 들어오면 다시 알린다

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
	//M24: 준비 상태가 구성을 기다리므로 보통은 오지 않는다. 체크포인트 이어하기(Resting부터)의 안전장치
	if (!CanStartWaves())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Wave %d not started. No server wave plan."), CurrentWaveIndex + 2);
		return;
	}

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

void AWarriorStageGameMode::UploadWaveCheckpoint()
{
	if (IsCurrentWaveLast() || CurrentWaveIndex < 0)
	{
		return;
	}

	UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
	if (!StagePlay || StagePlay->GetCurrentStagePlayId().IsEmpty())
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Stage] Checkpoint not uploaded. No stage play id (offline or start failed)."));
		return;
	}

	FWarriorPlayCheckpoint Checkpoint;
	FString Error;
	if (!WarriorPlayCheckpoint::Capture(this, Checkpoint, Error))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Checkpoint not captured: %s"), *Error);
		return;
	}
	//웨이브 번호는 GameMode 기준(판 통계의 마지막 클리어와 같지만, 건너뛴 웨이브가 있어도 진행 위치와 맞게)
	Checkpoint.ClearedWave = CurrentWaveIndex + 1;
	//시간은 결과 제출과 같은 기준(이어한 판이면 체크포인트 시간부터 이어짐)
	Checkpoint.ElapsedSeconds = GetRunPlayTimeSeconds();

	const FString StagePlayId = StagePlay->GetCurrentStagePlayId();
	const int32 ClearedWave = Checkpoint.ClearedWave;
	const bool bSent = StagePlay->SaveCheckpoint(Checkpoint, [StagePlayId, ClearedWave](int32 Status, const FString& ErrorCode)
	{
		if (Status == 200)
		{
			UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Checkpoint saved. stagePlayId=%s clearedWave=%d"), *StagePlayId, ClearedWave);
			return;
		}
		//재시도·보관하지 않는다. 다음 웨이브 클리어 때 새로 올리고, 이어하기는 서버에 마지막으로 저장된 것으로 한다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Checkpoint not saved. stagePlayId=%s clearedWave=%d status=%d code=%s"),
			*StagePlayId, ClearedWave, Status, *ErrorCode);
	});

	if (bSent)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Checkpoint sent. %s"), *WarriorPlayCheckpoint::Describe(Checkpoint));
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

	//로그인 때 이어하기를 골랐으면 그 정보를 꺼낸다(한 번만). 체크포인트가 있으면 판 안 값을 넣고 다음 웨이브의 쉬는 시간부터 시작한다
	FWarriorStageResumeInfo ResumeInfo;
	UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this);
	const bool bResume = LaunchSubsystem && LaunchSubsystem->ConsumeResumeInfo(GetResolvedStageId(), ResumeInfo);

	//① 서버에 스테이지 플레이 시작(응답을 기다리지 않는다). 받은 stagePlayId로 FinishRun에서 결과를 낸다.
	//이어하기면 시작 요청 없이 그 플레이 ID를 그대로 쓰고, 로그인 때 받은 웨이브 구성이 여기서 바로 들어온다(M24, HandleWavePlanChanged)
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this))
	{
		StagePlay->StartStagePlay(GetResolvedStageId(), bResume ? &ResumeInfo : nullptr);
	}
	//구독이 없거나(서브시스템 없음) 이미 정해진 상태도 같은 판단을 한다
	HandleWavePlanChanged();

	//② 체크포인트는 웨이브 구성(총 웨이브 수)이 들어온 뒤에 넣는다. 먼저 넣으면 총 0으로 보고 경고만 남긴 채 1웨이브부터 시작한다(M24)
	bResumingFromCheckpoint = bResume && ResumeInfo.Checkpoint.IsSet() && CanStartWaves() && ApplyResumeCheckpoint(ResumeInfo.Checkpoint);
}

bool AWarriorStageGameMode::ApplyResumeCheckpoint(const FWarriorPlayCheckpoint& InCheckpoint)
{
	const AWarriorStageFlowManager* FlowManager = GetStageFlowManager();
	const int32 TotalWaveCount = FlowManager ? FlowManager->GetTotalWaveCount() : 0;

	//마지막 웨이브를 클리어한 체크포인트는 없다(그때는 결과를 낸다). 범위를 벗어나면 처음부터
	if (InCheckpoint.ClearedWave <= 0 || InCheckpoint.ClearedWave >= TotalWaveCount)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Checkpoint ignored. clearedWave %d is out of range (total %d). Starting from wave 1."),
			InCheckpoint.ClearedWave, TotalWaveCount);
		return false;
	}

	FString Error;
	if (!WarriorPlayCheckpoint::Restore(this, InCheckpoint, Error))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Checkpoint not restored (%s). Starting from wave 1."), *Error);
		return false;
	}

	//클리어한 웨이브에 머문 것처럼 맞춘다. 쉬는 시간이 끝나면 StartNextWave가 그다음 웨이브를 시작한다.
	//판 통계는 위 Restore에서 같은 웨이브를 가리키게 되었으므로 아래 웨이브 알림으로 새 웨이브를 열지 않는다
	CurrentWaveIndex = InCheckpoint.ClearedWave - 1;
	if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
	{
		StageGameState->SetWaveInfo(InCheckpoint.ClearedWave, TotalWaveCount, FlowManager->IsBossWave(CurrentWaveIndex));
	}

	//이어한 판의 플레이 시간 = 체크포인트까지의 시간 + 이어한 뒤 시간(결정 F1). BeginRun이 정한 시작 시각을 그만큼 당긴다
	RunStartRealTime = GetWorld()->GetRealTimeSeconds() - FMath::Max(0.f, InCheckpoint.ElapsedSeconds);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Resuming from checkpoint. Cleared wave %d/%d, next wave %d starts after rest. Play time continues from %.1f s."),
		InCheckpoint.ClearedWave, TotalWaveCount, InCheckpoint.ClearedWave + 1, InCheckpoint.ElapsedSeconds);
	return true;
}

float AWarriorStageGameMode::GetRunPlayTimeSeconds() const
{
	return static_cast<float>(GetWorld()->GetRealTimeSeconds() - RunStartRealTime);
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
	Result.PlayTimeSeconds = GetRunPlayTimeSeconds();

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
	if (!InFlowManager)
	{
		return;
	}

	//개발용 로컬 웨이브: 스테이지 목록의 웨이브 → 레벨 FlowManager의 웨이브. 둘 다 에디터 전용이라 쿠킹한 빌드에는 없다
	UWarriorStageWaveDataAsset* WaveData = nullptr;
	if (!ResolvedLaunchInfo.WaveData.IsNull())
	{
		WaveData = ResolvedLaunchInfo.WaveData.LoadSynchronous();
		if (!WaveData)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Wave data %s for stage %s could not be loaded. Trying the level's own waves."),
				*ResolvedLaunchInfo.WaveData.ToString(),
				*ResolvedLaunchInfo.StageId.ToString());
		}
	}
	if (!WaveData)
	{
		WaveData = InFlowManager->GetLocalWaveData();
	}
	if (!WaveData)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] No local wave data for stage %s."), *ResolvedLaunchInfo.StageId.ToString());
		return;
	}

	InFlowManager->SetStageWaveData(WaveData);
}

//~ Begin 서버 웨이브 구성(M24)

void AWarriorStageGameMode::HandleWavePlanChanged()
{
	//BeginRun 전(지난 판의 늦은 알림)이거나 이미 정했으면 무시한다
	if (!RunId.IsValid() || bRunStartFailed || bWavePlanApplied || bUsingLocalWaves)
	{
		return;
	}

	const UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
	const EWarriorWavePlanState State = StagePlay ? StagePlay->GetWavePlanState() : EWarriorWavePlanState::NoServerPlay;

	switch (State)
	{
	case EWarriorWavePlanState::Ready:
		ApplyServerWavePlan(StagePlay->GetWaveData(), StagePlay->GetWavePlan().WaveVersion);
		break;

	case EWarriorWavePlanState::Failed:
		FailRunStart(StagePlay->GetWavePlanError());
		return;

	case EWarriorWavePlanState::NoServerPlay:
		//서버 플레이가 없는 판(로그인 없이 연 PIE, 디버그 이어하기)만 로컬 웨이브로 대신한다. 서버 판에는 섞지 않는다
		if (UWarriorStageSettings::CanUseLocalWaveFallback())
		{
			bUsingLocalWaves = true;
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] No server play. Using local wave data (development only)."));
			if (AWarriorStageFlowManager* FlowManager = GetStageFlowManager())
			{
				ApplyStageWaveData(FlowManager);
				if (CurrentWaveIndex < 0)
				{
					if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
					{
						StageGameState->SetWaveInfo(0, FlowManager->GetTotalWaveCount(), false);
					}
				}
			}
			break;
		}
		{
			FWarriorApiError Error;
			Error.Code = TEXT("NETWORK_ERROR");
			FailRunStart(UWarriorStagePlaySubsystem::MakeStageStartError(Error));
		}
		return;

	default:
		//None·Waiting: 아직 모른다
		return;
	}

	//준비 시간이 끝나 구성을 기다리던 중이었으면 이제 첫 웨이브로 간다
	if (bWaitingForWavePlan && CanStartWaves())
	{
		bWaitingForWavePlan = false;
		GetWorldTimerManager().ClearTimer(WavePlanWaitHandle);
		SendStageEvent(EWarriorStageEvent::StateTimerElapsed);
	}
}

void AWarriorStageGameMode::ApplyServerWavePlan(const TArray<FWarriorStageWaveData>& InWaves, const FString& InWaveVersion)
{
	AWarriorStageFlowManager* FlowManager = GetStageFlowManager();
	if (!FlowManager)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Server wave plan %s not applied. No flow manager."), *InWaveVersion);
		return;
	}

	if (!ServerWaveData)
	{
		ServerWaveData = NewObject<UWarriorStageWaveDataAsset>(this, TEXT("ServerWaveData"), RF_Transient);
	}
	ServerWaveData->SetWaves(InWaves);
	FlowManager->SetStageWaveData(ServerWaveData);
	bWavePlanApplied = true;

	//첫 웨이브 전이면 총 웨이브 수를 서버 값으로 다시 알린다(체크포인트 이어하기는 ApplyResumeCheckpoint가 이어서 맞춘다)
	if (CurrentWaveIndex < 0)
	{
		if (AWarriorStageGameState* StageGameState = GetGameState<AWarriorStageGameState>())
		{
			StageGameState->SetWaveInfo(0, FlowManager->GetTotalWaveCount(), false);
		}
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Server wave plan applied. waveVersion=%s waves=%d"), *InWaveVersion, FlowManager->GetTotalWaveCount());
}

void AWarriorStageGameMode::WaitForWavePlan()
{
	if (bWaitingForWavePlan || bRunStartFailed)
	{
		return;
	}

	const UWarriorStageSettings* Settings = GetDefault<UWarriorStageSettings>();
	const float Limit = FMath::Max(Settings ? Settings->ServerWaveWaitSeconds : 0.f, KINDA_SMALL_NUMBER);
	bWaitingForWavePlan = true;
	GetWorldTimerManager().SetTimer(WavePlanWaitHandle, this, &ThisClass::HandleWavePlanTimeout, Limit, false);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Prepare time is over. Waiting for the server wave plan up to %.1f s."), Limit);
}

void AWarriorStageGameMode::HandleWavePlanTimeout()
{
	bWaitingForWavePlan = false;
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Server wave plan did not arrive in time."));

	//서브시스템을 실패로 바꾸면 알림으로 FailRunStart가 불린다. 늦게 온 시작 응답은 서브시스템이 바로 포기한다
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this))
	{
		StagePlay->FailWavePlan(TEXT("STAGE_WAVE_PLAN_TIMEOUT"));
		return;
	}

	FWarriorApiError Error;
	Error.Code = TEXT("STAGE_WAVE_PLAN_TIMEOUT");
	FailRunStart(UWarriorStagePlaySubsystem::MakeStageStartError(Error));
}

void AWarriorStageGameMode::FailRunStart(const FWarriorApiResult& InError)
{
	if (bRunStartFailed)
	{
		return;
	}
	bRunStartFailed = true;
	bWaitingForWavePlan = false;
	GetWorldTimerManager().ClearTimer(WavePlanWaitHandle);
	StopAllWaves();

	//시작하지 않은 판이라 결과를 내지 않는다(플레이어가 쓰러져 끝 상태로 가도 FinishRun이 보내지 않게)
	bRunFinished = true;

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Run not started. code=%s. Returning to the main menu."), *InError.Error.Code);

	//구성을 쓸 수 없는 서버 판(모르는 몬스터·구성 없음)은 진행 중으로 남아 로그인마다 묻지 않게 포기한다
	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this))
	{
		if (!StagePlay->GetCurrentStagePlayId().IsEmpty())
		{
			StagePlay->AbandonCurrentStagePlay();
		}
	}

	//팝업은 레벨이 바뀌어도 메인메뉴에 다시 뜬다(UWarriorErrorPresenterSubsystem). 메인메뉴 이동은 여기서 한다(팝업 담당은 Quit만 실행)
	if (UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this))
	{
		Api->PresentError(InError);
	}
	if (AWarriorStagePlayerController* StagePlayerController = Cast<AWarriorStagePlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		StagePlayerController->ReturnToMainMenu();
	}
}

//~ End 서버 웨이브 구성

