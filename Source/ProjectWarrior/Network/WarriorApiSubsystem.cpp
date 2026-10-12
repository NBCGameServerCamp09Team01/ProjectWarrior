// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorApiSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorApiSettings.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"

#define LOCTEXT_NAMESPACE "WarriorApi"

namespace WarriorApi
{
	//성공 응답 { data: { account: … } }에서 계정 스냅샷을 꺼낸다(결과 제출 응답 등). 없으면 false
	bool ReadAccountSnapshot(const FString& InBody, FWarriorAccountSnapshotDto& OutSnapshot)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InBody);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* Data = nullptr;
		const TSharedPtr<FJsonObject>* AccountObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("data"), Data) || !(*Data)->TryGetObjectField(TEXT("account"), AccountObject))
		{
			return false;
		}

		return FJsonObjectConverter::JsonObjectToUStruct((*AccountObject).ToSharedRef(), &OutSnapshot);
	}
}

UWarriorApiSubsystem* UWarriorApiSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorApiSubsystem>() : nullptr;
}

void UWarriorApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//로그인 성공이 보관 요청 재전송의 시작 신호다
	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleLoginCompleted);
	}
}

void UWarriorApiSubsystem::Deinitialize()
{
	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &ThisClass::HandleLoginCompleted);
	}

	//코어 티커는 이 서브시스템보다 오래 살므로 반드시 뗀다
	for (TPair<int32, FCall>& Pair : Calls)
	{
		if (Pair.Value.RetryTicker.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(Pair.Value.RetryTicker);
		}
	}
	Calls.Reset();

	Super::Deinitialize();
}

//~ Begin 오류 해석

FWarriorApiError UWarriorApiSubsystem::ParseError(int32 InStatus, const FString& InBody)
{
	FWarriorApiError Error;
	if (InStatus == 0)
	{
		Error.Code = TEXT("NETWORK_ERROR");
		Error.Retryable = true;
	}
	else if (!FJsonObjectConverter::JsonObjectStringToUStruct(InBody, &Error) || Error.Code.IsEmpty())
	{
		Error = FWarriorApiError();
		Error.Code = FString::Printf(TEXT("HTTP_%d"), InStatus);
	}

	Error.HttpStatus = InStatus;
	return Error;
}

bool UWarriorApiSubsystem::IsRetryable(const FWarriorApiError& InError)
{
	//본문 없는 5xx(프록시 502·504 등)는 retryable 칸이 없어도 다시 보낸다(README 재전송 규칙)
	return InError.HttpStatus == 0
		|| InError.Retryable
		|| (InError.HttpStatus >= 500 && InError.Code.StartsWith(TEXT("HTTP_")));
}

//~ End 오류 해석

//~ Begin 요청

bool UWarriorApiSubsystem::Send(const FWarriorApiRequest& InRequest, TFunction<void(const FWarriorApiResult&)>&& OnDone)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] Request not sent. No auth subsystem. %s %s"), *InRequest.Verb, *InRequest.Path);
		return false;
	}

	if (InRequest.bAuth && !Auth->IsLoggedIn())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] Request not sent. Not logged in. %s %s"), *InRequest.Verb, *InRequest.Path);
		return false;
	}

	const int32 CallId = NextCallId++;
	FCall& Call = Calls.Add(CallId);
	Call.Id = CallId;
	Call.Request = InRequest;
	Call.OnDone = MoveTemp(OnDone);
	Call.SessionSerial = Auth->GetSessionSerial();
	Call.AccountId = Auth->GetAccount().AccountId;

	if (!Dispatch(CallId))
	{
		Calls.Remove(CallId);
		return false;
	}
	return true;
}

bool UWarriorApiSubsystem::Dispatch(int32 InCallId)
{
	FCall* Call = Calls.Find(InCallId);
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Call || !Auth)
	{
		return false;
	}

	Call->RetryTicker.Reset();

	//재시도를 기다리는 사이 로그인 상태가 끝났다(로그아웃·401). 보낼 수 없으니 보관 대상이면 보관하고 끝낸다
	if (Call->Request.bAuth && (!Auth->IsLoggedIn() || Auth->GetSessionSerial() != Call->SessionSerial))
	{
		FWarriorApiResult Result;
		Result.Attempts = Call->Attempts;
		Result.Error.Code = TEXT("AUTH_SESSION_NOT_FOUND");
		Result.Error.HttpStatus = 401;
		Result.HttpStatus = 401;
		Result.Rule = FWarriorErrorText::Resolve(Result.Error, Call->Request.Context);
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] Session ended before retry. %s %s"), *Call->Request.Verb, *Call->Request.Path);
		Finish(InCallId, Result, true);
		return true;
	}

	++Call->Attempts;
	const bool bStarted = Auth->SendHttp(Call->Request.Verb, Call->Request.Path, Call->Request.Body, Call->Request.bAuth,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InCallId](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleResponse(InCallId, Status, Body);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] Request could not be started. %s %s"), *Call->Request.Verb, *Call->Request.Path);
	}
	return bStarted;
}

void UWarriorApiSubsystem::HandleResponse(int32 InCallId, int32 InStatus, const FString& InBody)
{
	FCall* Call = Calls.Find(InCallId);
	if (!Call)
	{
		return;
	}

	FWarriorApiResult Result;
	Result.HttpStatus = InStatus;
	Result.Body = InBody;
	Result.Attempts = Call->Attempts;

	//── 2xx
	if (InStatus >= 200 && InStatus < 300)
	{
		Result.bOk = true;
		Finish(InCallId, Result, false);
		return;
	}

	Result.Error = ParseError(InStatus, InBody);
	Result.Rule = FWarriorErrorText::Resolve(Result.Error, Call->Request.Context);
	Call->LastResult = Result;

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] %s %s failed. status=%d code=%s retryable=%s attempt=%d"),
		*Call->Request.Verb, *Call->Request.Path, InStatus, *Result.Error.Code,
		IsRetryable(Result.Error) ? TEXT("true") : TEXT("false"), Call->Attempts);
	if (Result.Rule.bClientBug)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Api] %s looks like a game-side bug. Check the request against the spec. %s %s"),
			*Result.Error.Code, *Call->Request.Verb, *Call->Request.Path);
	}

	//── 401: 보관 대상이면 먼저 보관하고, 보낼 때와 같은 세션이면 로그인 상태를 끝낸다
	if (InStatus == 401 && Call->Request.bAuth)
	{
		Result.bStored = Call->bStored || StoreCall(*Call);
		Call->bStored = Result.bStored;
		const int32 SentSessionSerial = Call->SessionSerial;
		const FWarriorApiError Error = Result.Error;
		Finish(InCallId, Result, false);

		if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
		{
			Auth->NotifyUnauthorized(Error, SentSessionSerial);
		}
		return;
	}

	//── 다시 보내도 되는 실패: 설정 간격으로 자동 재시도
	if (IsRetryable(Result.Error))
	{
		//보관 대상은 첫 실패에서 바로 보관한다. 재시도·묻기를 기다리는 사이 게임이 꺼져도 결과가 남는다(성공하면 Finish가 지운다)
		if (!Call->bStored)
		{
			Call->bStored = StoreCall(*Call);
		}

		//응답 없음(서버가 꺼짐)과 서버 장애는 간격을 따로 둔다
		const UWarriorApiSettings* Settings = UWarriorApiSettings::Get();
		const TArray<float>& Delays = InStatus == 0 ? Settings->NetworkErrorRetryDelaysSeconds : Settings->AutoRetryDelaysSeconds;
		if (Call->AutoRetries < Delays.Num())
		{
			const float Delay = FMath::Max(0.f, Delays[Call->AutoRetries]);
			++Call->AutoRetries;
			Call->RetryTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
				[this, InCallId](float)
				{
					if (!Dispatch(InCallId))
					{
						HandleResponse(InCallId, 0, FString());
					}
					return false;
				}), Delay);

			UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Retrying in %.1fs (%d/%d). %s %s"),
				Delay, Call->AutoRetries, Delays.Num(), *Call->Request.Verb, *Call->Request.Path);
			return;
		}

		HandleRetriesExhausted(InCallId, Result);
		return;
	}

	//── 다시 보내도 같은 실패
	Finish(InCallId, Result, false);
}

void UWarriorApiSubsystem::HandleRetriesExhausted(int32 InCallId, const FWarriorApiResult& InResult)
{
	FCall* Call = Calls.Find(InCallId);
	if (!Call)
	{
		return;
	}

	if (!Call->Request.bAskOnFail)
	{
		Finish(InCallId, InResult, true);
		return;
	}

	const UWarriorApiSettings* Settings = UWarriorApiSettings::Get();
	const bool bCanAsk = Settings->bAskRetryAfterAutoRetry && !Call->bAsked && PromptCallId == 0 && OnNoticeRequested.IsBound();
	if (!bCanAsk)
	{
		//이미 물었거나 물을 수 없다 → 보관 후 종료 안내
		FWarriorApiResult Result = InResult;
		const bool bStored = Call->bStored || StoreCall(*Call);
		Result.bStored = bStored;
		Call->bStored = bStored;
		Finish(InCallId, Result, false);
		RequestQuitNotice(Result, bStored);
		return;
	}

	//묻기 전에 먼저 보관한다(답을 기다리는 사이 게임이 꺼져도 결과가 남는다). 다시 시도가 성공하면 지운다
	Call->bAsked = true;
	Call->bStored = Call->bStored || StoreCall(*Call);
	PromptCallId = InCallId;

	//버튼이 "다시 시도 / 게임 종료"이므로 "잠시 뒤 다시 시도해 주세요" 같은 문장은 넣지 않고, 무슨 일인지와 결과가 어떻게 됐는지만 쓴다
	FWarriorErrorNotice Notice;
	Notice.Code = InResult.Error.Code;
	const FText Headline = InResult.HttpStatus == 0
		? LOCTEXT("AskHeadlineNetwork", "서버에 연결할 수 없습니다.")
		: LOCTEXT("AskHeadlineServer", "서버가 잠시 응답하지 않습니다.");
	Notice.Message = Call->bStored
		? FText::Format(LOCTEXT("AskRetryStored", "{0}\n\n결과는 이 PC에 보관했습니다.\n다시 시도하거나 게임을 종료해 주세요."), Headline)
		: FText::Format(LOCTEXT("AskRetry", "{0}\n\n다시 시도하거나 게임을 종료해 주세요."), Headline);
	Notice.Action = EWarriorErrorAction::Retry;
	Notice.bAskRetry = true;
	RequestNotice(Notice);
}

void UWarriorApiSubsystem::AnswerRetryPrompt(bool bRetry)
{
	const int32 CallId = PromptCallId;
	PromptCallId = 0;

	FCall* Call = Calls.Find(CallId);
	if (!Call)
	{
		return;
	}

	if (bRetry)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Player chose retry. %s %s"), *Call->Request.Verb, *Call->Request.Path);
		Call->AutoRetries = 0;
		if (!Dispatch(CallId))
		{
			HandleResponse(CallId, 0, FString());
		}
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Player chose quit. %s %s"), *Call->Request.Verb, *Call->Request.Path);
	FWarriorApiResult Result = Call->LastResult;
	Result.bStored = Call->bStored;
	Finish(CallId, Result, true);
	RequestQuitNotice(Result, Result.bStored);
}

void UWarriorApiSubsystem::Finish(int32 InCallId, FWarriorApiResult InResult, bool bStoreOnFail)
{
	FCall Call;
	if (!Calls.RemoveAndCopyValue(InCallId, Call))
	{
		return;
	}

	if (Call.RetryTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Call.RetryTicker);
	}

	if (InResult.bOk)
	{
		//묻기 전에 보관해 둔 것이 있으면 이제 필요 없다
		if (Call.bStored)
		{
			FWarriorPendingRequestStore::Remove(Call.AccountId, Call.Request.RequestId);
		}
	}
	else
	{
		InResult.bStored = Call.bStored || (bStoreOnFail && StoreCall(Call));
	}

	if (Call.OnDone)
	{
		Call.OnDone(InResult);
	}
}

bool UWarriorApiSubsystem::StoreCall(const FCall& InCall)
{
	if (InCall.Request.StoreKind == EWarriorPendingKind::None)
	{
		return false;
	}

	FWarriorPendingRequest Entry;
	Entry.Kind = InCall.Request.StoreKind;
	Entry.Verb = InCall.Request.Verb;
	Entry.Path = InCall.Request.Path;
	Entry.Body = InCall.Request.Body;
	Entry.RequestId = InCall.Request.RequestId;
	Entry.StoreKey = InCall.Request.StoreKey;
	Entry.CreatedAt = FDateTime::UtcNow();
	Entry.ExpiresAt = InCall.Request.StoreExpiresAt.GetTicks() > 0
		? InCall.Request.StoreExpiresAt
		: Entry.CreatedAt + FTimespan::FromHours(UWarriorApiSettings::Get()->PendingDefaultTtlHours);

	return FWarriorPendingRequestStore::Save(InCall.AccountId, Entry);
}

//~ End 요청

//~ Begin 안내

void UWarriorApiSubsystem::PresentError(const FWarriorApiResult& InResult)
{
	if (InResult.bOk)
	{
		return;
	}

	if (InResult.Rule.Presentation != EWarriorErrorPresentation::Popup)
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Api] %s is not a popup error. The screen shows it."), *InResult.Error.Code);
		return;
	}

	FWarriorErrorNotice Notice;
	Notice.Code = InResult.Error.Code;
	Notice.Message = InResult.Rule.Text;
	Notice.Action = InResult.Rule.Action;
	RequestNotice(Notice);
}

void UWarriorApiSubsystem::RequestNotice(const FWarriorErrorNotice& InNotice)
{
	if (!OnNoticeRequested.IsBound())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] No popup presenter. Notice not shown: %s"), *InNotice.Message.ToString());
		return;
	}

	OnNoticeRequested.Broadcast(InNotice);
}

void UWarriorApiSubsystem::RequestQuitNotice(const FWarriorApiResult& InResult, bool bStored)
{
	FWarriorErrorNotice Notice;
	Notice.Code = InResult.Error.Code;
	Notice.Message = bStored
		? LOCTEXT("QuitStored", "서버에 연결할 수 없어\n게임을 종료합니다.\n\n보관한 결과는 다음에 로그인하면\n자동으로 저장됩니다.")
		: LOCTEXT("Quit", "서버에 연결할 수 없어\n게임을 종료합니다.");
	Notice.Action = EWarriorErrorAction::Quit;
	RequestNotice(Notice);
}

void UWarriorApiSubsystem::CheckServerOnTitle()
{
	const UWarriorApiSettings* Settings = UWarriorApiSettings::Get();
	if (bServerChecked || !Settings->bCheckServerOnTitle)
	{
		return;
	}
	bServerChecked = true;

	auto OnServerDown = [this](int32 InStatus)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] Server check failed. status=%d. Quitting after notice."), InStatus);
		FWarriorErrorNotice Notice;
		Notice.Code = InStatus == 0 ? TEXT("NETWORK_ERROR") : FString::Printf(TEXT("HTTP_%d"), InStatus);
		Notice.Message = LOCTEXT("ServerDown", "서버에 연결할 수 없습니다.\n게임을 종료합니다.");
		Notice.Action = EWarriorErrorAction::Quit;
		RequestNotice(Notice);
	};

	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	const bool bStarted = Auth && Auth->SendHttp(TEXT("GET"), Settings->HealthCheckPath, FString(), false,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), OnServerDown](int32 Status, const FString& Body)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (Status == 200)
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Server check OK."));
				return;
			}
			OnServerDown(Status);
		}, Settings->HealthCheckTimeoutSeconds);

	if (!bStarted)
	{
		OnServerDown(0);
	}
}

//~ End 안내

//~ Begin 보관 요청 재전송

void UWarriorApiSubsystem::HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	if (bSuccess)
	{
		ResendPending();
	}
}

void UWarriorApiSubsystem::ResendPending()
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (bResendingPending || !Auth || !Auth->IsLoggedIn())
	{
		return;
	}

	ResendAccountId = Auth->GetAccount().AccountId;
	int32 CorruptedCount = 0;
	ResendQueue = FWarriorPendingRequestStore::LoadAll(ResendAccountId, CorruptedCount);
	if (ResendQueue.IsEmpty() && CorruptedCount == 0)
	{
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Resending %d pending request(s). corrupted=%d"), ResendQueue.Num(), CorruptedCount);

	bResendingPending = true;
	ResendSessionSerial = Auth->GetSessionSerial();
	ResendSent = 0;
	ResendRejected = CorruptedCount;
	ResendRejectReasons.Reset();
	if (CorruptedCount > 0)
	{
		ResendRejectReasons.AddUnique(FWarriorErrorText::GetText(TEXT("PENDING_CORRUPTED")).ToString());
	}

	ResendNext();
}

void UWarriorApiSubsystem::ResendNext()
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || !Auth->IsLoggedIn() || Auth->GetSessionSerial() != ResendSessionSerial)
	{
		//로그인 상태가 끝났다. 남은 파일은 다음 로그인 때 보낸다
		UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Resend stopped. Session ended. Remaining files are kept."));
		bResendingPending = false;
		ResendQueue.Reset();
		return;
	}

	if (ResendQueue.IsEmpty())
	{
		FinishResend();
		return;
	}

	const FWarriorPendingRequest Entry = ResendQueue[0];
	ResendQueue.RemoveAt(0);

	if (Entry.ExpiresAt < FDateTime::UtcNow())
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Pending request expired. requestId=%s expiresAt=%s"), *Entry.RequestId, *Entry.ExpiresAt.ToIso8601());
		FWarriorPendingRequestStore::Remove(ResendAccountId, Entry.RequestId);
		++ResendRejected;
		ResendRejectReasons.AddUnique(FWarriorErrorText::GetText(TEXT("PENDING_EXPIRED")).ToString());
		ResendNext();
		return;
	}

	//같은 requestId·같은 본문으로 보낸다. 이미 보관돼 있으므로 다시 보관하지 않고, 묻지 않는다
	FWarriorApiRequest Request;
	Request.Verb = Entry.Verb;
	Request.Path = Entry.Path;
	Request.Body = Entry.Body;
	Request.RequestId = Entry.RequestId;
	Request.bAuth = true;

	const bool bStarted = Send(Request, [WeakThis = TWeakObjectPtr<ThisClass>(this), Entry](const FWarriorApiResult& Result)
	{
		UWarriorApiSubsystem* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (Result.bOk)
		{
			UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Pending request sent. requestId=%s status=%d"), *Entry.RequestId, Result.HttpStatus);
			FWarriorPendingRequestStore::Remove(This->ResendAccountId, Entry.RequestId);
			++This->ResendSent;

			//결과 제출처럼 응답에 계정 값이 있으면 메인메뉴 값을 새로 맞춘다.
			//응답이 오는 사이 로그아웃·다른 계정 로그인이 있었으면 적용하지 않는다(그 계정의 값이 아니다)
			FWarriorAccountSnapshotDto Snapshot;
			UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(This);
			if (Auth && Auth->IsLoggedIn() && Auth->GetSessionSerial() == This->ResendSessionSerial
				&& WarriorApi::ReadAccountSnapshot(Result.Body, Snapshot) && Snapshot.AccountId == This->ResendAccountId)
			{
				Auth->ApplyAccountSnapshot(Snapshot);
			}
		}
		else if (Result.HttpStatus == 401)
		{
			//로그인 상태가 끝났다. 파일은 남기고 ResendNext가 멈춘다
		}
		else if (IsRetryable(Result.Error))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Api] Pending request still failing. Kept for next login. requestId=%s code=%s"),
				*Entry.RequestId, *Result.Error.Code);
		}
		else
		{
			//서버가 다시 받지 않는 요청(만료·검사 실패 등). 지우고 이유를 모은다
			FWarriorPendingRequestStore::Remove(This->ResendAccountId, Entry.RequestId);
			++This->ResendRejected;
			This->ResendRejectReasons.AddUnique(Result.Rule.Text.ToString());
		}

		This->ResendNext();
	});

	if (!bStarted)
	{
		bResendingPending = false;
		ResendQueue.Reset();
	}
}

void UWarriorApiSubsystem::FinishResend()
{
	bResendingPending = false;
	UE_LOG(LogProjectWarrior, Log, TEXT("[Api] Resend finished. sent=%d rejected=%d"), ResendSent, ResendRejected);

	OnPendingResendFinished.Broadcast(ResendSent, ResendRejected);

	if (ResendSent == 0 && ResendRejected == 0)
	{
		return;
	}

	//덩어리마다 빈 줄로 나눈다: 저장한 것 / 저장하지 못한 것 / 이유(이유 문구는 문구 표에서 이미 짧은 줄로 끊겨 있다)
	TArray<FString> Blocks;
	if (ResendSent > 0)
	{
		Blocks.Add(FText::Format(LOCTEXT("ResendSent", "보관해 둔 결과 {0}개를\n서버에 저장했습니다."), ResendSent).ToString());
	}
	if (ResendRejected > 0)
	{
		Blocks.Add(FText::Format(LOCTEXT("ResendRejected", "보관해 둔 결과 {0}개를\n저장하지 못했습니다."), ResendRejected).ToString());
		Blocks.Append(ResendRejectReasons);
	}

	FWarriorErrorNotice Notice;
	Notice.Code = ResendRejected > 0 ? TEXT("PENDING_REJECTED") : FString();
	Notice.Message = FText::FromString(FString::Join(Blocks, TEXT("\n\n")));
	Notice.Action = EWarriorErrorAction::None;
	RequestNotice(Notice);
}

//~ End 보관 요청 재전송

#undef LOCTEXT_NAMESPACE
