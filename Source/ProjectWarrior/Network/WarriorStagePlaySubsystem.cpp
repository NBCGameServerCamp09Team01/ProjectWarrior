// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStagePlaySubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorAccountSync.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorPendingRequestStore.h"
#include "ProjectWarrior/Network/WarriorStagePlayTypes.h"
#include "ProjectWarrior/Stage/WarriorPlayCheckpoint.h"
#include "ProjectWarrior/Stage/WarriorMonsterCatalogDataAsset.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"

namespace WarriorStagePlayApi
{
	const TCHAR* StagePlaysPath = TEXT("/accounts/me/stage-plays");
}

UWarriorStagePlaySubsystem* UWarriorStagePlaySubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorStagePlaySubsystem>() : nullptr;
}

void UWarriorStagePlaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UWarriorApiSubsystem>();
	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
}

void UWarriorStagePlaySubsystem::Deinitialize()
{
	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}

	Super::Deinitialize();
}

void UWarriorStagePlaySubsystem::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	//다른 계정이 이 플레이 ID로 결과를 내지 않게 비운다. 늦게 온 시작 응답도 버리도록 번호를 올린다
	++StartSerial;
	CurrentStagePlayId.Reset();
	CurrentStagePlayExpiresAt = FDateTime();
	ClearWavePlan();
}

void UWarriorStagePlaySubsystem::StartStagePlay(FName InStageId, const FWarriorStageResumeInfo* InResume)
{
	//이전 플레이 ID로 결과를 내지 않도록 먼저 비운다
	++StartSerial;
	CurrentStagePlayId.Reset();
	CurrentStagePlayExpiresAt = FDateTime();
	//M24: 이전 판(다시 하기·409 복구) 구성이 남지 않게 비운다. 이번 판 구성은 아래에서 넣는다
	ClearWavePlan();

	//── 이어하기: 서버의 진행 중 플레이를 그대로 쓴다. 결과 검사의 경과 시간·마감은 원래 시작 기준이다(stage-play-api.md "이어하기(1차)")
	if (InResume && InResume->IsSet())
	{
		CurrentStagePlayId = InResume->StagePlayId.ToLower();
		if (!FDateTime::ParseIso8601(*InResume->ExpiresAt, CurrentStagePlayExpiresAt))
		{
			CurrentStagePlayExpiresAt = FDateTime();
		}
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Resumed. stagePlayId=%s stage=%s fromWave=%d expiresAt=%s (no start request)"),
			*CurrentStagePlayId, *InStageId.ToString(), InResume->ResumeFromWave, *InResume->ExpiresAt);

		//M24: 로그인 때 받은 P2의 구성(시작 때 고정)을 바로 넣는다. 서버 플레이 없는 디버그 이어하기는 구성이 없다
		if (CurrentStagePlayId.IsEmpty())
		{
			SetWavePlanState(EWarriorWavePlanState::NoServerPlay);
		}
		else
		{
			ApplyWavePlan(InResume->WavePlan);
		}
		return;
	}

	SetWavePlanState(EWarriorWavePlanState::Waiting);
	SendStart(InStageId.ToString().ToLower(), true);
}

bool UWarriorStagePlaySubsystem::RequestCurrentStagePlay(FCurrentStagePlayCallback InCallback)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		return false;
	}

	return Auth->SendAuthorized(TEXT("GET"), FString(WarriorStagePlayApi::StagePlaysPath) + TEXT("/current"), FString(),
		[Callback = MoveTemp(InCallback)](int32 Status, const FString& Body)
		{
			FWarriorStagePlayResponseDto ResponseDto;
			if (Status == 204)
			{
				Callback(EWarriorCurrentStagePlay::None, ResponseDto.Data);
				return;
			}

			if (Status == 200 && FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto) && !ResponseDto.Data.StagePlayId.IsEmpty())
			{
				//M24: 시작 때 고정한 웨이브 구성. 이어하기 정보에 실어 레벨에서 그대로 쓴다
				WarriorWavePlan::ReadFromResponse(Body, ResponseDto.Data.WavePlan);
				Callback(EWarriorCurrentStagePlay::InProgress, ResponseDto.Data);
				return;
			}

			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Current play could not be read. status=%d code=%s"),
				Status, *UWarriorApiSubsystem::ParseError(Status, Body).Code);
			Callback(EWarriorCurrentStagePlay::Failed, FWarriorStagePlayDto());
		});
}

bool UWarriorStagePlaySubsystem::AbandonStagePlay(const FString& InStagePlayId, FAbandonCallback InCallback)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || InStagePlayId.IsEmpty())
	{
		return false;
	}

	const FString Path = FString::Printf(TEXT("%s/%s/abandon"), WarriorStagePlayApi::StagePlaysPath, *InStagePlayId);
	return Auth->SendAuthorized(TEXT("POST"), Path, FString(),
		[Callback = MoveTemp(InCallback)](int32 Status, const FString& Body)
		{
			Callback(Status, Status == 200 ? FString() : UWarriorApiSubsystem::ParseError(Status, Body).Code);
		});
}

bool UWarriorStagePlaySubsystem::SaveCheckpoint(const FWarriorPlayCheckpoint& InCheckpoint, FCheckpointSaveCallback InCallback)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || CurrentStagePlayId.IsEmpty())
	{
		return false;
	}

	FString RequestBody;
	if (!WarriorPlayCheckpoint::ToJson(InCheckpoint, RequestBody))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] Checkpoint body could not be built. stagePlayId=%s"), *CurrentStagePlayId);
		return false;
	}

	const FString Path = FString::Printf(TEXT("%s/%s/checkpoint"), WarriorStagePlayApi::StagePlaysPath, *CurrentStagePlayId);
	return Auth->SendAuthorized(TEXT("PUT"), Path, RequestBody,
		[Callback = MoveTemp(InCallback)](int32 Status, const FString& Body)
		{
			Callback(Status, Status == 200 ? FString() : UWarriorApiSubsystem::ParseError(Status, Body).Code);
		});
}

bool UWarriorStagePlaySubsystem::RequestCheckpoint(const FString& InStagePlayId, FCheckpointFetchCallback InCallback)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || InStagePlayId.IsEmpty())
	{
		return false;
	}

	const FString Path = FString::Printf(TEXT("%s/%s/checkpoint"), WarriorStagePlayApi::StagePlaysPath, *InStagePlayId);
	return Auth->SendAuthorized(TEXT("GET"), Path, FString(),
		[Callback = MoveTemp(InCallback), InStagePlayId](int32 Status, const FString& Body)
		{
			FWarriorPlayCheckpointResponseDto ResponseDto;
			if (Status == 204)
			{
				Callback(EWarriorCheckpointFetch::None, ResponseDto.Data);
				return;
			}

			if (Status == 200 && FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto))
			{
				Callback(EWarriorCheckpointFetch::Found, ResponseDto.Data);
				return;
			}

			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Checkpoint could not be read. stagePlayId=%s status=%d code=%s"),
				*InStagePlayId, Status, *UWarriorApiSubsystem::ParseError(Status, Body).Code);
			Callback(EWarriorCheckpointFetch::Failed, FWarriorPlayCheckpoint());
		});
}

void UWarriorStagePlaySubsystem::AbandonCurrentStagePlay()
{
	if (CurrentStagePlayId.IsEmpty())
	{
		return;
	}

	//결과를 내지 않게 먼저 비운다. 늦게 온 시작 응답도 버린다
	const FString StagePlayId = CurrentStagePlayId;
	++StartSerial;
	CurrentStagePlayId.Reset();
	CurrentStagePlayExpiresAt = FDateTime();

	const bool bStarted = AbandonStagePlay(StagePlayId, [StagePlayId](int32 Status, const FString& ErrorCode)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Abandon by leaving. stagePlayId=%s status=%d code=%s"), *StagePlayId, Status, *ErrorCode);
	});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Abandon by leaving not sent. stagePlayId=%s"), *StagePlayId);
	}
}

void UWarriorStagePlaySubsystem::SendStart(const FString& InStageId, bool bInRecoverInProgress)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start not sent. No auth subsystem. Stage %s"), *InStageId);
		SetWavePlanState(EWarriorWavePlanState::NoServerPlay);
		return;
	}

	FWarriorStagePlayStartRequestDto RequestDto;
	RequestDto.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	RequestDto.StageId = InStageId;

	FString RequestBody;
	if (!FJsonObjectConverter::UStructToJsonObjectString(RequestDto, RequestBody, 0, 0, 0, nullptr, false))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] Start request body could not be built. Stage %s"), *InStageId);
		return;
	}

	const int32 SentStartSerial = StartSerial;
	const bool bStarted = Auth->SendAuthorized(TEXT("POST"), WarriorStagePlayApi::StagePlaysPath, RequestBody,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), SentStartSerial, InStageId, bInRecoverInProgress](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleStartResponse(SentStartSerial, InStageId, bInRecoverInProgress, Status, Body);
			}
		});

	if (!bStarted)
	{
		//로그인하지 않은 상태(로그인 화면을 거치지 않은 PIE 등). 이번 판은 서버에 저장되지 않는다.
		//처음 시작이면 서버 플레이가 없는 판(개발 빌드는 로컬 웨이브), 409 복구 중이면 그사이 로그인이 끝난 것이라 시작하지 않는다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start not sent. Stage %s. This run will not be saved."), *InStageId);
		if (bInRecoverInProgress)
		{
			SetWavePlanState(EWarriorWavePlanState::NoServerPlay);
		}
		else
		{
			FailWavePlan(TEXT("NETWORK_ERROR"));
		}
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Start sent. Stage %s requestId=%s"), *InStageId, *RequestDto.RequestId);
}

void UWarriorStagePlaySubsystem::HandleStartResponse(int32 InStartSerial, const FString& InStageId, bool bInRecoverInProgress, int32 Status, const FString& Body)
{
	if (!IsCurrentStart(InStartSerial))
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[StagePlay] Start response for an old start ignored. status=%d"), Status);
		return;
	}

	//── 201: 새 플레이(같은 requestId를 다시 보낸 경우도 같은 플레이로 201)
	if (Status == 201)
	{
		FWarriorStagePlayResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto) || ResponseDto.Data.StagePlayId.IsEmpty())
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] Start response could not be read. Stage %s"), *InStageId);
			FailWavePlan(TEXT("INVALID_RESPONSE"));
			return;
		}

		//M24: 구성을 기다리다 이미 포기한 판(기다림 한도를 넘김)이면 서버에 남지 않게 바로 포기한다
		if (WavePlanState == EWarriorWavePlanState::Failed)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start response came after the run was given up. Abandoning play %s."),
				*ResponseDto.Data.StagePlayId);
			AbandonStagePlay(ResponseDto.Data.StagePlayId, [](int32, const FString&) {});
			return;
		}

		CurrentStagePlayId = ResponseDto.Data.StagePlayId;
		if (!FDateTime::ParseIso8601(*ResponseDto.Data.ExpiresAt, CurrentStagePlayExpiresAt))
		{
			CurrentStagePlayExpiresAt = FDateTime();
		}
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Started. stagePlayId=%s stage=%s waveCount=%d expiresAt=%s serverRequestId=%s"),
			*CurrentStagePlayId, *ResponseDto.Data.StageId, ResponseDto.Data.WaveCount, *ResponseDto.Data.ExpiresAt, *ResponseDto.Meta.RequestId);

		//M24: 시작 때 고정한 웨이브 구성
		WarriorWavePlan::ReadFromResponse(Body, ResponseDto.Data.WavePlan);
		ApplyWavePlan(ResponseDto.Data.WavePlan);
		return;
	}

	const FWarriorApiError Error = UWarriorApiSubsystem::ParseError(Status, Body);

	//── 409 STAGE_PLAY_IN_PROGRESS: 이전 판(중간에 끈 판 등)이 남아 있다. 보관 결과가 있으면 그것부터, 없으면 포기하고 한 번만 다시 시작한다
	if (Status == 409 && Error.Code == TEXT("STAGE_PLAY_IN_PROGRESS") && bInRecoverInProgress)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] A previous play is still in progress. Recovering it and starting again."));
		RecoverInProgress(InStartSerial, InStageId);
		return;
	}

	//── 그 밖(404 STAGE_NOT_FOUND·403 STAGE_LOCKED·401·연결 실패 등): 이번 판은 서버에 저장되지 않는다.
	//401은 Auth가 이미 로그인 상태를 끝냈다. 안내 후 메인메뉴는 S5 3단계
	UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start failed. Stage %s status=%d code=%s. This run will not be saved."),
		*InStageId, Status, *Error.Code);

	//M24: 서버 구성이 없으므로 이 판은 시작하지 않는다(안내 후 메인메뉴). 401은 Auth가 로그인 상태를 끝내 컨트롤러가 메인메뉴로 간다
	if (Status != 401)
	{
		FailWavePlanWithError(Error);
	}
}

void UWarriorStagePlaySubsystem::RecoverInProgress(int32 InStartSerial, const FString& InStageId)
{
	const bool bStarted = RequestCurrentStagePlay(
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InStartSerial, InStageId](EWarriorCurrentStagePlay Result, const FWarriorStagePlayDto& Play)
		{
			if (!WeakThis.IsValid() || !WeakThis->IsCurrentStart(InStartSerial))
			{
				return;
			}

			//── 204: 그사이 마감·포기로 끝났다. 바로 다시 시작
			if (Result == EWarriorCurrentStagePlay::None)
			{
				WeakThis->SendStart(InStageId, false);
				return;
			}

			if (Result == EWarriorCurrentStagePlay::InProgress)
			{
				//명세 P1: 그 플레이의 보관 결과가 있으면 포기하지 말고 결과부터 보낸다
				const UWarriorAuthSubsystem* AuthNow = UWarriorAuthSubsystem::Get(WeakThis.Get());
				FWarriorPendingRequest Stored;
				if (AuthNow && FWarriorPendingRequestStore::FindByKey(AuthNow->GetAccount().AccountId, EWarriorPendingKind::StageResult,
					Play.StagePlayId, Stored))
				{
					WeakThis->ResendStoredThenRestart(InStartSerial, Stored, Play.StagePlayId, InStageId);
					return;
				}

				WeakThis->AbandonAndRestart(InStartSerial, Play.StagePlayId, InStageId);
				return;
			}

			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Current play could not be read. This run will not be saved."));
			WeakThis->FailWavePlan(TEXT("NETWORK_ERROR"));
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Current play request not sent. This run will not be saved."));
		FailWavePlan(TEXT("NETWORK_ERROR"));
	}
}

void UWarriorStagePlaySubsystem::ResendStoredThenRestart(int32 InStartSerial, const FWarriorPendingRequest& InStored, const FString& InStagePlayId, const FString& InStageId)
{
	UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this);
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Api || !Auth)
	{
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Sending the stored result of play %s before starting again. requestId=%s"),
		*InStagePlayId, *InStored.RequestId);

	//보관본 그대로(같은 requestId·같은 본문). 이미 보관돼 있으므로 다시 보관하지 않는다
	FWarriorApiRequest Request;
	Request.Verb = InStored.Verb;
	Request.Path = InStored.Path;
	Request.Body = InStored.Body;
	Request.RequestId = InStored.RequestId;

	const FString AccountId = Auth->GetAccount().AccountId;
	const int32 SentSessionSerial = Auth->GetSessionSerial();
	const FString StoredRequestId = InStored.RequestId;
	const bool bStarted = Api->Send(Request,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InStartSerial, StoredRequestId, InStagePlayId, InStageId, AccountId, SentSessionSerial](const FWarriorApiResult& Result)
		{
			if (Result.bOk || (Result.HttpStatus != 401 && !UWarriorApiSubsystem::IsRetryable(Result.Error)))
			{
				//받아들여졌거나(플레이가 끝남) 서버가 다시 받지 않는다. 어느 쪽이든 보관본은 필요 없다
				FWarriorPendingRequestStore::Remove(AccountId, StoredRequestId);
			}

			if (!WeakThis.IsValid() || !WeakThis->IsCurrentStart(InStartSerial) || Result.HttpStatus == 401)
			{
				return;
			}

			if (Result.bOk)
			{
				//보관 결과가 받아들여졌다. 새 판을 시작하기 전에 응답의 계정 값(지금 값)을 사본에 맞춘다(동기화 시점 T5b)
				FWarriorStageResultSubmitResponseDto ResponseDto;
				if (FJsonObjectConverter::JsonObjectStringToUStruct(Result.Body, &ResponseDto))
				{
					WeakThis->ApplyResultAccount(InStagePlayId, SentSessionSerial, AccountId, ResponseDto.Data.Account);
				}
				WeakThis->SendStart(InStageId, false);
				return;
			}

			//거절(만료 등)되었거나 아직 보낼 수 없다. 그 플레이를 포기하고 새로 시작한다(아직 보낼 수 없는 결과는 보관본이 남아 다음 로그인 때 다시 본다)
			WeakThis->AbandonAndRestart(InStartSerial, InStagePlayId, InStageId);
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Stored result could not be sent. This run will not be saved."));
		FailWavePlan(TEXT("NETWORK_ERROR"));
	}
}

void UWarriorStagePlaySubsystem::AbandonAndRestart(int32 InStartSerial, const FString& InStagePlayId, const FString& InStageId)
{
	const bool bStarted = AbandonStagePlay(InStagePlayId,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InStartSerial, InStagePlayId, InStageId](int32 Status, const FString& ErrorCode)
		{
			if (!WeakThis.IsValid() || !WeakThis->IsCurrentStart(InStartSerial))
			{
				return;
			}

			//── 200 포기됨, 409 STAGE_PLAY_NOT_IN_PROGRESS 그사이 이미 끝남: 둘 다 이제 시작할 수 있다
			if (Status == 200 || (Status == 409 && ErrorCode == TEXT("STAGE_PLAY_NOT_IN_PROGRESS")))
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Previous play %s ended. Starting again."), *InStagePlayId);
				WeakThis->SendStart(InStageId, false);
				return;
			}

			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Abandon failed. stagePlayId=%s status=%d code=%s. This run will not be saved."),
				*InStagePlayId, Status, *ErrorCode);
			if (Status != 401)
			{
				WeakThis->FailWavePlan(Status == 0 ? TEXT("NETWORK_ERROR") : (ErrorCode.IsEmpty() ? TEXT("UNKNOWN") : *ErrorCode));
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Abandon request not sent. This run will not be saved."));
		FailWavePlan(TEXT("NETWORK_ERROR"));
	}
}

void UWarriorStagePlaySubsystem::SubmitResult(const FWarriorStageResult& InResult)
{
	if (CurrentStagePlayId.IsEmpty())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not sent. No stagePlayId (start failed or no response yet). Stage %s"),
			*InResult.StageId.ToString());
		return;
	}

	UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this);
	if (!Api)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not sent. No api subsystem. stagePlayId=%s"), *CurrentStagePlayId);
		return;
	}

	//칸 이름을 손으로 맞춘다(bCleared → cleared). 재전송도 같은 본문(같은 requestId)을 쓴다
	FWarriorStageResultRequestDto RequestDto;
	RequestDto.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	RequestDto.StageId = InResult.StageId.ToString().ToLower();
	RequestDto.Difficulty = InResult.Difficulty;
	RequestDto.Cleared = InResult.bCleared;
	RequestDto.ReachedWave = InResult.ReachedWave;
	RequestDto.TotalWaveCount = InResult.TotalWaveCount;
	RequestDto.PlayTimeSeconds = InResult.PlayTimeSeconds;
	RequestDto.EarnedGold = InResult.EarnedGold;
	RequestDto.KillCount = InResult.KillCount;

	FString RequestBody;
	if (!FJsonObjectConverter::UStructToJsonObjectString(RequestDto, RequestBody, 0, 0, 0, nullptr, false))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] Result request body could not be built. stagePlayId=%s"), *CurrentStagePlayId);
		return;
	}

	//S5: 계속 실패하면 결과를 보관하고(종류 StageResult, 키 stagePlayId, 유효 기간 = 플레이 마감), 다음 로그인 때 다시 보낸다
	FWarriorApiRequest Request;
	Request.Verb = TEXT("POST");
	Request.Path = FString::Printf(TEXT("%s/%s/result"), WarriorStagePlayApi::StagePlaysPath, *CurrentStagePlayId);
	Request.Body = RequestBody;
	Request.RequestId = RequestDto.RequestId;
	Request.StoreKind = EWarriorPendingKind::StageResult;
	Request.StoreKey = CurrentStagePlayId;
	Request.StoreExpiresAt = CurrentStagePlayExpiresAt;
	Request.bAskOnFail = true;

	//응답이 오는 사이 로그아웃·다시 로그인이 있었는지 보려고 보낼 때의 세션·계정을 기억한다(재전송 길과 같은 조건)
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	const int32 SentSessionSerial = Auth ? Auth->GetSessionSerial() : 0;
	const FString SentAccountId = Auth ? Auth->GetAccount().AccountId : FString();

	const FString StagePlayId = CurrentStagePlayId;
	const bool bStarted = Api->Send(Request,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), StagePlayId, SentSessionSerial, SentAccountId](const FWarriorApiResult& Result)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->HandleResultResult(StagePlayId, SentSessionSerial, SentAccountId, Result);
		}
	});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not sent (not logged in or request failed to start). stagePlayId=%s"), *StagePlayId);
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Result sent. stagePlayId=%s %s wave %d/%d playTime %.1f s requestId=%s"),
		*StagePlayId, InResult.bCleared ? TEXT("cleared") : TEXT("failed"), InResult.ReachedWave, InResult.TotalWaveCount,
		InResult.PlayTimeSeconds, *RequestDto.RequestId);
}

void UWarriorStagePlaySubsystem::HandleResultResult(const FString& InStagePlayId, int32 InSentSessionSerial, const FString& InSentAccountId,
	const FWarriorApiResult& InResult)
{
	//── 200: SAVED 또는 ALREADY_SAVED. 둘 다 서버에 저장된 것
	if (InResult.bOk)
	{
		FWarriorStageResultSubmitResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(InResult.Body, &ResponseDto))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result saved but the response could not be read. stagePlayId=%s"), *InStagePlayId);
			return;
		}

		const FWarriorStageRewardDto& Reward = ResponseDto.Data.Result.Reward;
		//serverRequestId는 응답 meta.requestId(서버 로그 번호). 보낼 때 로그의 requestId(재전송 키)와 다르다
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Result saveStatus %s. stagePlayId=%s cleared=%s exp +%d level %d->%d statPoints +%d attempts=%d serverRequestId=%s"),
			*ResponseDto.Data.SaveStatus, *InStagePlayId, ResponseDto.Data.Result.Cleared ? TEXT("true") : TEXT("false"),
			Reward.ExpGained, Reward.LevelBefore, Reward.LevelAfter, Reward.StatPointsGained, InResult.Attempts, *ResponseDto.Meta.RequestId);

		//결과 응답의 계정 값(지금 값)으로 메인메뉴를 맞춘다(동기화 시점 T2). 게임이 먼저 계산한 보상 값을 서버 값이 덮는다
		ApplyResultAccount(InStagePlayId, InSentSessionSerial, InSentAccountId, ResponseDto.Data.Account);
		return;
	}

	FString FieldErrors;
	for (const FWarriorApiFieldError& FieldError : InResult.Error.Errors)
	{
		FieldErrors += FString::Printf(TEXT(" [%s: %s]"), *FieldError.Field, *FieldError.Message);
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not saved. stagePlayId=%s status=%d code=%s stored=%s attempts=%d%s"),
		*InStagePlayId, InResult.HttpStatus, *InResult.Error.Code, InResult.bStored ? TEXT("true") : TEXT("false"), InResult.Attempts, *FieldErrors);

	//── 보관했다(재시도까지 실패·401): 종료 안내·세션 종료 안내는 Api·Auth가 이미 띄웠다
	//── 서버가 거절했다(RESULT_INVALID·STAGE_PLAY_EXPIRED 등): 문구 표의 안내를 띄운다
	if (!InResult.bStored && InResult.HttpStatus != 401 && !UWarriorApiSubsystem::IsRetryable(InResult.Error))
	{
		if (UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this))
		{
			Api->PresentError(InResult);
		}
	}
}

bool UWarriorStagePlaySubsystem::ApplyResultAccount(const FString& InStagePlayId, int32 InSentSessionSerial, const FString& InSentAccountId,
	const FWarriorAccountSnapshotDto& InAccount)
{
	//보낸 뒤 로그아웃·다시 로그인했거나 다른 계정 값이면(응답에 account가 없으면 빈 ID) 적용하지 않는다. version 비교는 ApplyAccountSnapshot이 한다
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || !WarriorAccountSync::IsSameSession(Auth->IsLoggedIn(), Auth->GetSessionSerial(), InSentSessionSerial,
		InSentAccountId, InAccount.AccountId))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result account not applied (logged out, logged in again or no account). stagePlayId=%s"),
			*InStagePlayId);
		return false;
	}

	Auth->ApplyAccountSnapshot(InAccount);
	return true;
}

//~ Begin 웨이브 구성(M24)

void UWarriorStagePlaySubsystem::ClearWavePlan()
{
	WavePlanState = EWarriorWavePlanState::None;
	CurrentWavePlan = FWarriorStageWavePlan();
	CurrentWaveData.Reset();
	WavePlanError = FWarriorApiResult();
}

void UWarriorStagePlaySubsystem::ApplyWavePlan(const FWarriorStageWavePlan& InPlan)
{
	CurrentWavePlan = InPlan;
	if (!InPlan.IsSet())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] Wave plan is missing in the play response. stagePlayId=%s"), *CurrentStagePlayId);
		FailWavePlan(TEXT("STAGE_WAVE_PLAN_MISSING"));
		return;
	}

	const UWarriorMonsterCatalogDataAsset* Catalog = UWarriorMonsterCatalogDataAsset::LoadFromSettings();
	if (!Catalog)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] MonsterCatalog is not set in Project Settings > Warrior Stage."));
	}

	FName UnknownMonster;
	if (!WarriorWavePlan::ToWaveData(InPlan, Catalog, CurrentWaveData, UnknownMonster))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StagePlay] Monster %s is not in this build's monster catalog. stagePlayId=%s waveVersion=%s"),
			*UnknownMonster.ToString(), *CurrentStagePlayId, *InPlan.WaveVersion);
		FailWavePlan(TEXT("STAGE_WAVE_MONSTER_UNKNOWN"));
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Wave plan ready. stagePlayId=%s waveVersion=%s waves=%d"),
		*CurrentStagePlayId, *InPlan.WaveVersion, CurrentWaveData.Num());
	SetWavePlanState(EWarriorWavePlanState::Ready);
}

void UWarriorStagePlaySubsystem::SetWavePlanState(EWarriorWavePlanState InState)
{
	WavePlanState = InState;
	OnWavePlanChanged.Broadcast();
}

void UWarriorStagePlaySubsystem::FailWavePlan(const FString& InGameErrorCode)
{
	FWarriorApiError Error;
	Error.Code = InGameErrorCode;
	FailWavePlanWithError(Error);
}

void UWarriorStagePlaySubsystem::FailWavePlanWithError(const FWarriorApiError& InError)
{
	//한 판에 한 번만 안내한다(예: 기다림 한도를 넘긴 뒤 늦게 온 실패 응답)
	if (WavePlanState == EWarriorWavePlanState::Failed)
	{
		return;
	}

	CurrentWaveData.Reset();
	WavePlanError = MakeStageStartError(InError);
	UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] This run will not start. code=%s status=%d"), *InError.Code, InError.HttpStatus);
	SetWavePlanState(EWarriorWavePlanState::Failed);
}

FWarriorApiResult UWarriorStagePlaySubsystem::MakeStageStartError(const FWarriorApiError& InError)
{
	FWarriorApiResult Result;
	Result.bOk = false;
	Result.HttpStatus = InError.HttpStatus;
	Result.Error = InError;
	Result.Rule = FWarriorErrorText::Resolve(InError);
	//문구만 표에서 가져온다. 판 시작 실패는 메인메뉴에 안내할 화면 칸이 없어 늘 팝업이고,
	//이 레벨에서 다시 시도할 플레이가 없어 다음 행동은 코드와 관계없이 메인메뉴다(stage-play-api.md "웨이브 구성")
	Result.Rule.Presentation = EWarriorErrorPresentation::Popup;
	Result.Rule.Action = EWarriorErrorAction::GoMainMenu;
	return Result;
}

//~ End 웨이브 구성

