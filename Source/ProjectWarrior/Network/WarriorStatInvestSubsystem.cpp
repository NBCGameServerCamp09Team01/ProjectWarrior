// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStatInvestSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorStatInvestTypes.h"

#define LOCTEXT_NAMESPACE "WarriorStatInvest"

namespace WarriorStatInvestApi
{
	const TCHAR* Path = TEXT("/accounts/me/stat-investments");

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

UWarriorStatInvestSubsystem* UWarriorStatInvestSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorStatInvestSubsystem>() : nullptr;
}

bool UWarriorStatInvestSubsystem::RequestInvest(const TMap<FGameplayTag, int32>& InChanges)
{
	if (bInFlight)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StatInvest] Request ignored. Previous one is still in flight."));
		return false;
	}

	//0은 빼고, 태그 이름순으로 정렬해 본문을 늘 같은 모양으로 만든다
	TArray<TPair<FGameplayTag, int32>> Changes;
	for (const TPair<FGameplayTag, int32>& Pair : InChanges)
	{
		if (Pair.Key.IsValid() && Pair.Value != 0)
		{
			Changes.Add(Pair);
		}
	}
	if (Changes.IsEmpty())
	{
		return false;
	}
	Changes.Sort([](const TPair<FGameplayTag, int32>& A, const TPair<FGameplayTag, int32>& B)
	{
		return A.Key.GetTagName().LexicalLess(B.Key.GetTagName());
	});

	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || !Auth->IsLoggedIn())
	{
		//로컬 저장·반영 길은 두지 않는다(d 10/12). 화면에 안내만 띄운다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StatInvest] Not logged in. Request not sent."));
		Finish(false, TEXT("NOT_LOGGED_IN"), ErrorToText(TEXT("NOT_LOGGED_IN"), 0));
		return false;
	}

	FWarriorStatInvestRequestDto RequestDto;
	RequestDto.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	for (const TPair<FGameplayTag, int32>& Pair : Changes)
	{
		FWarriorStatInvestEntryDto& Entry = RequestDto.Investments.AddDefaulted_GetRef();
		Entry.StatTag = Pair.Key.ToString();
		Entry.Points = Pair.Value;
	}

	FString RequestBody;
	if (!FJsonObjectConverter::UStructToJsonObjectString(RequestDto, RequestBody, 0, 0, 0, nullptr, false))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StatInvest] Request body could not be built."));
		return false;
	}

	bInFlight = true;
	if (!SendInvest(RequestBody, 1))
	{
		bInFlight = false;
		return false;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[StatInvest] Invest sent. entries=%d requestId=%s"), Changes.Num(), *RequestDto.RequestId);
	return true;
}

bool UWarriorStatInvestSubsystem::SendInvest(const FString& InRequestBody, int32 InAttempt)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		return false;
	}

	const bool bStarted = Auth->SendAuthorized(TEXT("POST"), WarriorStatInvestApi::Path, InRequestBody,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InRequestBody, InAttempt](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleInvestResponse(InRequestBody, InAttempt, Status, Body);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StatInvest] Request could not be sent (not logged in or failed to start)."));
	}
	return bStarted;
}

void UWarriorStatInvestSubsystem::HandleInvestResponse(const FString& InRequestBody, int32 InAttempt, int32 Status, const FString& Body)
{
	//── 200: SAVED 또는 ALREADY_SAVED. 응답 account를 로그인 응답과 같은 길로 적용한다(OnAccountChanged → 화면 갱신)
	if (Status == 200)
	{
		FWarriorStatInvestResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto))
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[StatInvest] Invest saved but the response could not be read."));
			Finish(false, TEXT("INVALID_RESPONSE"), ErrorToText(TEXT("INVALID_RESPONSE"), Status));
			return;
		}

		if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
		{
			Auth->ApplyAccountSnapshot(ResponseDto.Data.Account);
		}
		UE_LOG(LogProjectWarrior, Log, TEXT("[StatInvest] Invest saveStatus %s. statPoints=%d version=%lld serverRequestId=%s"),
			*ResponseDto.Data.SaveStatus, ResponseDto.Data.Result.StatPoints, ResponseDto.Data.Result.Version, *ResponseDto.Meta.RequestId);
		Finish(true, FString(), FText::GetEmpty());
		return;
	}

	const FWarriorApiError Error = WarriorStatInvestApi::ParseError(Status, Body);

	//── retryable: 같은 본문(같은 requestId)으로 다시. 401은 Auth가 이미 로그인 상태를 끝냈고 retryable이 아니다
	if (Error.Retryable && InAttempt < FMath::Max(1, InvestMaxAttempts))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StatInvest] Invest failed, sending again. status=%d code=%s attempt=%d"),
			Status, *Error.Code, InAttempt);
		if (SendInvest(InRequestBody, InAttempt + 1))
		{
			return;
		}
	}

	FString FieldErrors;
	for (const FWarriorApiFieldError& FieldError : Error.Errors)
	{
		FieldErrors += FString::Printf(TEXT(" [%s: %s]"), *FieldError.Field, *FieldError.Message);
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[StatInvest] Invest not saved. status=%d code=%s retryable=%s attempt=%d%s"),
		Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"), InAttempt, *FieldErrors);
	Finish(false, Error.Code, ErrorToText(Error.Code, Status));
}

void UWarriorStatInvestSubsystem::Finish(bool bInSuccess, const FString& InCode, const FText& InMessage)
{
	bInFlight = false;
	OnInvestCompleted.Broadcast(bInSuccess, InCode, InMessage);
}

FText UWarriorStatInvestSubsystem::ErrorToText(const FString& InCode, int32 InStatus)
{
	if (InCode == TEXT("STAT_POINTS_NOT_ENOUGH"))
	{
		return LOCTEXT("PointsNotEnough", "스탯 포인트가 부족합니다.");
	}
	if (InCode == TEXT("STAT_MAX_EXCEEDED"))
	{
		return LOCTEXT("MaxExceeded", "최대치를 넘는 스탯이 있습니다.");
	}
	if (InCode == TEXT("STAT_BELOW_ZERO"))
	{
		return LOCTEXT("BelowZero", "투자한 포인트보다 많이 내릴 수 없습니다.");
	}
	if (InCode == TEXT("NETWORK_ERROR") || InStatus >= 500 || InCode == TEXT("REQUEST_IN_PROGRESS") || InCode == TEXT("VERSION_CONFLICT"))
	{
		return LOCTEXT("TryLater", "잠시 뒤 다시 시도해 주세요.");
	}
	if (InCode == TEXT("NOT_LOGGED_IN"))
	{
		return LOCTEXT("NotLoggedIn", "로그인이 필요합니다.");
	}
	if (InStatus == 401)
	{
		return LOCTEXT("SessionEnded", "로그인이 끝났습니다. 다시 로그인해 주세요.");
	}
	//STAT_UNKNOWN·VALIDATION_FAILED·IDEMPOTENCY_KEY_REUSED 등: 게임 쪽 버그
	return LOCTEXT("CannotProcess", "요청을 처리할 수 없습니다.");
}

#undef LOCTEXT_NAMESPACE
