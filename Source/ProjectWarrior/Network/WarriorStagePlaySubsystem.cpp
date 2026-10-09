// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStagePlaySubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorStagePlayTypes.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"

namespace WarriorStagePlayApi
{
	const TCHAR* StagePlaysPath = TEXT("/accounts/me/stage-plays");

	//결과 제출은 처음 1번 + retryable일 때 1번 더(같은 세션 안). 보관 뒤 재전송은 다음 작업
	constexpr int32 ResultMaxAttempts = 2;

	//실패 응답 본문을 읽는다. 연결 실패(Status 0)는 NETWORK_ERROR, 본문을 못 읽으면 HTTP_<상태>
	FWarriorApiError ParseError(int32 InStatus, const FString& InBody)
	{
		FWarriorApiError Error;
		if (InStatus == 0)
		{
			Error.Code = TEXT("NETWORK_ERROR");
			Error.Retryable = true;
		}
		else if (!FJsonObjectConverter::JsonObjectStringToUStruct(InBody, &Error) || Error.Code.IsEmpty())
		{
			Error.Code = FString::Printf(TEXT("HTTP_%d"), InStatus);
		}
		return Error;
	}
}

UWarriorStagePlaySubsystem* UWarriorStagePlaySubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorStagePlaySubsystem>() : nullptr;
}

void UWarriorStagePlaySubsystem::StartStagePlay(FName InStageId)
{
	//이전 플레이 ID로 결과를 내지 않도록 먼저 비운다
	++StartSerial;
	CurrentStagePlayId.Reset();

	SendStart(InStageId.ToString().ToLower(), true);
}

void UWarriorStagePlaySubsystem::SendStart(const FString& InStageId, bool bInRecoverInProgress)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start not sent. No auth subsystem. Stage %s"), *InStageId);
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
		//로그인하지 않은 상태(로그인 화면을 거치지 않은 PIE 등). 이번 판은 서버에 저장되지 않는다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start not sent. Stage %s. This run will not be saved."), *InStageId);
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
			return;
		}

		CurrentStagePlayId = ResponseDto.Data.StagePlayId;
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Started. stagePlayId=%s stage=%s waveCount=%d serverRequestId=%s"),
			*CurrentStagePlayId, *ResponseDto.Data.StageId, ResponseDto.Data.WaveCount, *ResponseDto.Meta.RequestId);
		return;
	}

	const FWarriorApiError Error = WarriorStagePlayApi::ParseError(Status, Body);

	//── 409 STAGE_PLAY_IN_PROGRESS: 이전 판(중간에 끈 PIE 등)이 남아 있다. 포기하고 한 번만 다시 시작한다
	if (Status == 409 && Error.Code == TEXT("STAGE_PLAY_IN_PROGRESS") && bInRecoverInProgress)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] A previous play is still in progress. Abandoning it and starting again."));
		RecoverInProgress(InStartSerial, InStageId);
		return;
	}

	//── 그 밖(404 STAGE_NOT_FOUND·403 STAGE_LOCKED·401·연결 실패 등): 이번 판은 서버에 저장되지 않는다.
	//401은 Auth가 이미 로그인 상태를 끝냈다
	UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Start failed. Stage %s status=%d code=%s. This run will not be saved."),
		*InStageId, Status, *Error.Code);
}

void UWarriorStagePlaySubsystem::RecoverInProgress(int32 InStartSerial, const FString& InStageId)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		return;
	}

	const bool bStarted = Auth->SendAuthorized(TEXT("GET"), FString(WarriorStagePlayApi::StagePlaysPath) + TEXT("/current"), FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InStartSerial, InStageId](int32 Status, const FString& Body)
		{
			if (!WeakThis.IsValid() || !WeakThis->IsCurrentStart(InStartSerial))
			{
				return;
			}

			//── 204: 그사이 마감·포기로 끝났다. 바로 다시 시작
			if (Status == 204)
			{
				WeakThis->SendStart(InStageId, false);
				return;
			}

			FWarriorStagePlayResponseDto ResponseDto;
			if (Status == 200 && FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto) && !ResponseDto.Data.StagePlayId.IsEmpty())
			{
				WeakThis->AbandonAndRestart(InStartSerial, ResponseDto.Data.StagePlayId, InStageId);
				return;
			}

			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Current play could not be read. status=%d code=%s. This run will not be saved."),
				Status, *WarriorStagePlayApi::ParseError(Status, Body).Code);
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Current play request not sent. This run will not be saved."));
	}
}

void UWarriorStagePlaySubsystem::AbandonAndRestart(int32 InStartSerial, const FString& InStagePlayId, const FString& InStageId)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		return;
	}

	const FString Path = FString::Printf(TEXT("%s/%s/abandon"), WarriorStagePlayApi::StagePlaysPath, *InStagePlayId);
	const bool bStarted = Auth->SendAuthorized(TEXT("POST"), Path, FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InStartSerial, InStagePlayId, InStageId](int32 Status, const FString& Body)
		{
			if (!WeakThis.IsValid() || !WeakThis->IsCurrentStart(InStartSerial))
			{
				return;
			}

			//── 200 포기됨, 409 STAGE_PLAY_NOT_IN_PROGRESS 그사이 이미 끝남: 둘 다 이제 시작할 수 있다
			const FWarriorApiError Error = WarriorStagePlayApi::ParseError(Status, Body);
			if (Status == 200 || (Status == 409 && Error.Code == TEXT("STAGE_PLAY_NOT_IN_PROGRESS")))
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Previous play %s ended. Starting again."), *InStagePlayId);
				WeakThis->SendStart(InStageId, false);
				return;
			}

			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Abandon failed. stagePlayId=%s status=%d code=%s. This run will not be saved."),
				*InStagePlayId, Status, *Error.Code);
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Abandon request not sent. This run will not be saved."));
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

	if (SendResult(CurrentStagePlayId, RequestBody, 1))
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Result sent. stagePlayId=%s %s wave %d/%d playTime %.1f s requestId=%s"),
			*CurrentStagePlayId, InResult.bCleared ? TEXT("cleared") : TEXT("failed"), InResult.ReachedWave, InResult.TotalWaveCount,
			InResult.PlayTimeSeconds, *RequestDto.RequestId);
	}
}

bool UWarriorStagePlaySubsystem::SendResult(const FString& InStagePlayId, const FString& InRequestBody, int32 InAttempt)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not sent. No auth subsystem. stagePlayId=%s"), *InStagePlayId);
		return false;
	}

	const FString Path = FString::Printf(TEXT("%s/%s/result"), WarriorStagePlayApi::StagePlaysPath, *InStagePlayId);
	const bool bStarted = Auth->SendAuthorized(TEXT("POST"), Path, InRequestBody,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InStagePlayId, InRequestBody, InAttempt](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleResultResponse(InStagePlayId, InRequestBody, InAttempt, Status, Body);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not sent (not logged in or request failed to start). stagePlayId=%s"), *InStagePlayId);
	}
	return bStarted;
}

void UWarriorStagePlaySubsystem::HandleResultResponse(const FString& InStagePlayId, const FString& InRequestBody, int32 InAttempt, int32 Status, const FString& Body)
{
	//── 200: SAVED 또는 ALREADY_SAVED. 둘 다 서버에 저장된 것
	if (Status == 200)
	{
		FWarriorStageResultSubmitResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result saved but the response could not be read. stagePlayId=%s"), *InStagePlayId);
			return;
		}

		const FWarriorStageRewardDto& Reward = ResponseDto.Data.Result.Reward;
		//serverRequestId는 응답 meta.requestId(서버 로그 번호). 보낼 때 로그의 requestId(재전송 키)와 다르다
		UE_LOG(LogProjectWarrior, Log, TEXT("[StagePlay] Result saveStatus %s. stagePlayId=%s cleared=%s exp +%d level %d->%d statPoints +%d serverRequestId=%s"),
			*ResponseDto.Data.SaveStatus, *InStagePlayId, ResponseDto.Data.Result.Cleared ? TEXT("true") : TEXT("false"),
			Reward.ExpGained, Reward.LevelBefore, Reward.LevelAfter, Reward.StatPointsGained, *ResponseDto.Meta.RequestId);
		return;
	}

	const FWarriorApiError Error = WarriorStagePlayApi::ParseError(Status, Body);

	//── retryable(연결 실패·VERSION_CONFLICT·5xx): 같은 본문(같은 requestId)으로 한 번 더
	if (Error.Retryable && InAttempt < WarriorStagePlayApi::ResultMaxAttempts)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result failed, sending again. stagePlayId=%s status=%d code=%s attempt=%d"),
			*InStagePlayId, Status, *Error.Code, InAttempt);
		SendResult(InStagePlayId, InRequestBody, InAttempt + 1);
		return;
	}

	//── 다시 보내도 같은 것(RESULT_INVALID·RESULT_MISMATCH·STAGE_PLAY_EXPIRED 등) 또는 재시도도 실패. 401은 Auth가 이미 처리했다
	FString FieldErrors;
	for (const FWarriorApiFieldError& FieldError : Error.Errors)
	{
		FieldErrors += FString::Printf(TEXT(" [%s: %s]"), *FieldError.Field, *FieldError.Message);
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[StagePlay] Result not saved. stagePlayId=%s status=%d code=%s retryable=%s attempt=%d%s"),
		*InStagePlayId, Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"), InAttempt, *FieldErrors);
}
