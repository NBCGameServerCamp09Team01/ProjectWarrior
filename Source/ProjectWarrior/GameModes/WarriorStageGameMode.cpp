// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageGameMode.h"
#include "WarriorStageGameState.h"
#include "ProjectWarrior/Stage/WarriorStageFlowManager.h"
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
#include "TimerManager.h"



AWarriorStageGameMode::AWarriorStageGameMode()
{
	GameStateClass = AWarriorStageGameState::StaticClass();
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
		UE_LOG(LogTemp, Warning, TEXT("[Stage] Event %s ignored. State machine is not started."), *UEnum::GetValueAsString(InEvent));
		return;
	}

	const EWarriorStageState NextState = CurrentState->HandleEvent(InEvent);
	if (NextState == EWarriorStageState::None)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Stage] Event %s ignored in state %s"),
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
		UE_LOG(LogTemp, Error, TEXT("[Stage] Wave %d could not start. Skipping it."), CurrentWaveIndex + 1);

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
		UE_LOG(LogTemp, Error, TEXT("[Stage] No state object for %s"), *UEnum::GetValueAsString(InNewState));
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
		UE_LOG(LogTemp, Log, TEXT("[Stage] Waiting for initialize. FlowManager: %s, Player pawn: %s"),
			StageFlowManager.IsValid() ? TEXT("ready") : TEXT("missing"),
			bPlayerSpawned ? TEXT("ready") : TEXT("missing"));
		return;
	}

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