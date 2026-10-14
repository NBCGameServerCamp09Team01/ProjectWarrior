// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPlayCheckpointSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorPendingRequestStore.h"
#include "ProjectWarrior/Network/WarriorStageListSubsystem.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"
#include "ProjectWarrior/Stage/WarriorMonsterCatalogDataAsset.h"
#include "ProjectWarrior/Stage/WarriorPlayCheckpoint.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"

UWarriorPlayCheckpointSubsystem* UWarriorPlayCheckpointSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorPlayCheckpointSubsystem>() : nullptr;
}

void UWarriorPlayCheckpointSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UWarriorStagePlaySubsystem>();
	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleLoginCompleted);
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
	if (UWarriorApiSubsystem* Api = Collection.InitializeDependency<UWarriorApiSubsystem>())
	{
		Api->OnPendingResendFinished.AddUniqueDynamic(this, &ThisClass::HandleResendFinished);
	}
}

void UWarriorPlayCheckpointSubsystem::Deinitialize()
{
	ClearCheckTimer();

	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &ThisClass::HandleLoginCompleted);
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}
	if (UWarriorApiSubsystem* Api = GetGameInstance()->GetSubsystem<UWarriorApiSubsystem>())
	{
		Api->OnPendingResendFinished.RemoveDynamic(this, &ThisClass::HandleResendFinished);
	}

	Super::Deinitialize();
}

void UWarriorPlayCheckpointSubsystem::HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	if (!bSuccess)
	{
		return;
	}

	++CheckSerial;
	ResumablePlay = FWarriorStagePlayDto();
	ResumableCheckpoint = FWarriorPlayCheckpoint();
	SetState(EWarriorPlayCheckpointState::WaitingResend);

	ClearCheckTimer();
	GetGameInstance()->GetTimerManager().SetTimer(CheckTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleCheckTimeout), FMath::Max(1.f, CheckTimeoutSeconds), false);

	//재전송 쪽이 먼저 알림을 받아 이미 끝났을 수 있다(보낼 것이 없으면 그 자리에서 끝난다)
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (Auth && ResendFinishedSessionSerial == Auth->GetSessionSerial())
	{
		StartCheck();
	}
}

void UWarriorPlayCheckpointSubsystem::HandleResendFinished(int32 Sent, int32 Rejected)
{
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || !Auth->IsLoggedIn())
	{
		return;
	}

	ResendFinishedSessionSerial = Auth->GetSessionSerial();
	if (State == EWarriorPlayCheckpointState::WaitingResend)
	{
		StartCheck();
	}
}

void UWarriorPlayCheckpointSubsystem::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	//늦게 온 응답을 버리고 처음 상태로. 다음 로그인 때 다시 확인한다
	++CheckSerial;
	ClearCheckTimer();
	ResumablePlay = FWarriorStagePlayDto();
	ResumableCheckpoint = FWarriorPlayCheckpoint();
	SetState(EWarriorPlayCheckpointState::Idle);
}

void UWarriorPlayCheckpointSubsystem::StartCheck()
{
	UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
	if (!StagePlay)
	{
		ClearCheckTimer();
		SetState(EWarriorPlayCheckpointState::Blocked);
		return;
	}

	SetState(EWarriorPlayCheckpointState::Checking);

	//팝업에 스테이지 이름을 보이려고 목록도 받는다(실패해도 이름 대신 stageId를 보인다)
	if (UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		if (!StageList->IsRequesting())
		{
			StageList->RequestStageList();
		}
	}

	const int32 SentCheckSerial = CheckSerial;
	const bool bStarted = StagePlay->RequestCurrentStagePlay(
		[WeakThis = TWeakObjectPtr<ThisClass>(this), SentCheckSerial](EWarriorCurrentStagePlay Result, const FWarriorStagePlayDto& Play)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleCurrentStagePlay(SentCheckSerial, Result, Play);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Current play request not sent."));
		ClearCheckTimer();
		SetState(EWarriorPlayCheckpointState::Blocked);
	}
}

void UWarriorPlayCheckpointSubsystem::HandleCurrentStagePlay(int32 InCheckSerial, EWarriorCurrentStagePlay InResult, const FWarriorStagePlayDto& InPlay)
{
	if (InCheckSerial != CheckSerial || State != EWarriorPlayCheckpointState::Checking)
	{
		return;
	}

	switch (InResult)
	{
	case EWarriorCurrentStagePlay::None:
		UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] No play in progress."));
		FinishCheck(EWarriorPlayCheckpointState::None);
		return;

	case EWarriorCurrentStagePlay::InProgress:
		{
			//그 플레이의 결과가 아직 디스크에 있으면(재전송이 재시도할 수 있는 실패로 남음) 판은 이미 끝났다. 묻지 않고 결과를 먼저 보낸다
			const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
			FWarriorPendingRequest Stored;
			if (Auth && FWarriorPendingRequestStore::FindByKey(Auth->GetAccount().AccountId, EWarriorPendingKind::StageResult, InPlay.StagePlayId, Stored))
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Play %s has a stored result. Not asking to resume."), *InPlay.StagePlayId);
				FinishCheck(EWarriorPlayCheckpointState::Blocked);
				return;
			}

			ResumablePlay = InPlay;
			ResumableCheckpoint = FWarriorPlayCheckpoint();
			UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Play in progress. stagePlayId=%s stage=%s expiresAt=%s"),
				*InPlay.StagePlayId, *InPlay.StageId, *InPlay.ExpiresAt);

			//이어서 체크포인트(C2)를 묻는다. 확인 상태와 시간 상한은 그대로 이어진다
			UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
			const int32 SentCheckSerial = CheckSerial;
			const bool bStarted = StagePlay && StagePlay->RequestCheckpoint(InPlay.StagePlayId,
				[WeakThis = TWeakObjectPtr<ThisClass>(this), SentCheckSerial](EWarriorCheckpointFetch Result, const FWarriorPlayCheckpoint& Checkpoint)
				{
					if (WeakThis.IsValid())
					{
						WeakThis->HandleCheckpointFetched(SentCheckSerial, Result, Checkpoint);
					}
				});

			if (!bStarted)
			{
				UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Checkpoint request not sent. Resuming from wave 1."));
				FinishCheck(EWarriorPlayCheckpointState::Resumable);
			}
			return;
		}

	default:
		UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Current play could not be checked. Not asking to resume."));
		FinishCheck(EWarriorPlayCheckpointState::Blocked);
		return;
	}
}

void UWarriorPlayCheckpointSubsystem::HandleCheckpointFetched(int32 InCheckSerial, EWarriorCheckpointFetch InResult, const FWarriorPlayCheckpoint& InCheckpoint)
{
	if (InCheckSerial != CheckSerial || State != EWarriorPlayCheckpointState::Checking)
	{
		return;
	}

	//체크포인트가 없거나 쓸 수 없으면 처음(1웨이브)부터 이어 간다(1차와 같은 이어하기)
	if (InResult != EWarriorCheckpointFetch::Found)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] No usable checkpoint (%s). Resuming from wave 1."),
			InResult == EWarriorCheckpointFetch::None ? TEXT("none") : TEXT("request failed"));
	}
	else if (InCheckpoint.ContentVersion != FWarriorPlayCheckpoint::CurrentContentVersion)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Checkpoint content version %d is not %d. Resuming from wave 1."),
			InCheckpoint.ContentVersion, FWarriorPlayCheckpoint::CurrentContentVersion);
	}
	else if (!InCheckpoint.IsSet())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Checkpoint has no cleared wave. Resuming from wave 1."));
	}
	else
	{
		ResumableCheckpoint = InCheckpoint;
		UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Checkpoint found. Resuming from wave %d. %s"),
			GetResumeFromWave(), *WarriorPlayCheckpoint::Describe(ResumableCheckpoint));
	}

	FinishCheck(EWarriorPlayCheckpointState::Resumable);
}

void UWarriorPlayCheckpointSubsystem::FinishCheck(EWarriorPlayCheckpointState InState)
{
	ClearCheckTimer();
	SetState(InState);
}

void UWarriorPlayCheckpointSubsystem::HandleCheckTimeout()
{
	if (!IsChecking())
	{
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Check timed out (%s). Not asking to resume."), *UEnum::GetValueAsString(State));
	++CheckSerial;
	SetState(EWarriorPlayCheckpointState::Blocked);
}

bool UWarriorPlayCheckpointSubsystem::CanResumeInThisBuild() const
{
	if (ResumablePlay.StageId.IsEmpty())
	{
		return false;
	}

	FWarriorStageLaunchInfo Info;
	if (!UWarriorStageLaunchSubsystem::BuildLaunchInfo(FName(*ResumablePlay.StageId), Info) || Info.Level.IsNull())
	{
		return false;
	}

	//M24: 그 판의 웨이브 구성(시작 때 고정)을 이 빌드에서 만들 수 있어야 한다. 아니면 레벨을 열어도 시작하지 못하고,
	//플레이가 진행 중으로 남아 로그인마다 같은 실패가 되풀이되므로 팝업에서 [포기]만 보인다(stage-play-api.md "이어하기")
	FName UnknownMonster;
	if (!ResumablePlay.WavePlan.IsSet()
		|| WarriorWavePlan::FindUnknownMonster(ResumablePlay.WavePlan, UWarriorMonsterCatalogDataAsset::LoadFromSettings(), UnknownMonster))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Play %s cannot be resumed in this build. waves=%d unknownMonster=%s"),
			*ResumablePlay.StagePlayId, ResumablePlay.WavePlan.Waves.Num(), *UnknownMonster.ToString());
		return false;
	}
	return true;
}

bool UWarriorPlayCheckpointSubsystem::Resume()
{
	if (State != EWarriorPlayCheckpointState::Resumable || !CanResumeInThisBuild())
	{
		return false;
	}

	UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this);
	if (!LaunchSubsystem)
	{
		return false;
	}

	FWarriorStageResumeInfo ResumeInfo;
	ResumeInfo.StagePlayId = ResumablePlay.StagePlayId;
	ResumeInfo.ExpiresAt = ResumablePlay.ExpiresAt;
	ResumeInfo.ResumeFromWave = GetResumeFromWave();
	ResumeInfo.Checkpoint = ResumableCheckpoint;
	ResumeInfo.WavePlan = ResumablePlay.WavePlan;
	LaunchSubsystem->SelectStageForResume(FName(*ResumablePlay.StageId), ResumeInfo);

	UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Resume play %s (stage %s) from wave %d."),
		*ResumablePlay.StagePlayId, *ResumablePlay.StageId, ResumeInfo.ResumeFromWave);
	ResumablePlay = FWarriorStagePlayDto();
	ResumableCheckpoint = FWarriorPlayCheckpoint();
	SetState(EWarriorPlayCheckpointState::None);
	return true;
}

void UWarriorPlayCheckpointSubsystem::Abandon(TFunction<void(bool bEnded)> InOnDone)
{
	UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
	const FString StagePlayId = ResumablePlay.StagePlayId;
	if (State != EWarriorPlayCheckpointState::Resumable || !StagePlay)
	{
		if (InOnDone)
		{
			InOnDone(false);
		}
		return;
	}

	SetState(EWarriorPlayCheckpointState::Abandoning);

	const int32 SentCheckSerial = CheckSerial;
	const bool bStarted = StagePlay->AbandonStagePlay(StagePlayId,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), SentCheckSerial, StagePlayId, InOnDone](int32 Status, const FString& ErrorCode)
		{
			//200 포기함, 404 없음, 409 이미 끝남(결과·만료): 모두 서버에 진행 중 플레이가 더는 없다
			const bool bEnded = Status == 200 || Status == 404 || (Status == 409 && ErrorCode == TEXT("STAGE_PLAY_NOT_IN_PROGRESS"));
			UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Abandon play %s. status=%d code=%s ended=%s"),
				*StagePlayId, Status, *ErrorCode, bEnded ? TEXT("true") : TEXT("false"));

			if (!WeakThis.IsValid() || SentCheckSerial != WeakThis->CheckSerial)
			{
				return;
			}

			WeakThis->ResumablePlay = FWarriorStagePlayDto();
			WeakThis->ResumableCheckpoint = FWarriorPlayCheckpoint();
			//포기하지 못했으면 이번 로그인에는 다시 묻지 않는다(다음 로그인 때 묻는다)
			WeakThis->SetState(bEnded ? EWarriorPlayCheckpointState::None : EWarriorPlayCheckpointState::Blocked);
			if (InOnDone)
			{
				InOnDone(bEnded);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Abandon request not sent. stagePlayId=%s"), *StagePlayId);
		ResumablePlay = FWarriorStagePlayDto();
		ResumableCheckpoint = FWarriorPlayCheckpoint();
		SetState(EWarriorPlayCheckpointState::Blocked);
		if (InOnDone)
		{
			InOnDone(false);
		}
	}
}

void UWarriorPlayCheckpointSubsystem::SetState(EWarriorPlayCheckpointState InState)
{
	if (State == InState)
	{
		return;
	}

	State = InState;
	OnStateChanged.Broadcast(State);
}

void UWarriorPlayCheckpointSubsystem::ClearCheckTimer()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(CheckTimerHandle);
	}
}
