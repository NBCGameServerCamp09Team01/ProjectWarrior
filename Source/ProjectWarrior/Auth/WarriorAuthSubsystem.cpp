// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAuthSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTagContainer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebSocket.h"
#include "JsonObjectConverter.h"
#include "WebSocketsModule.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"

#define LOCTEXT_NAMESPACE "WarriorAuth"

namespace WarriorAuthRules
{
	//auth-api.md v1 A1 회원가입 요청 규칙. 명세가 바뀌면 같이 고친다(서버도 같은 규칙으로 다시 검증한다)
	constexpr int32 LoginIdMin = 4;
	constexpr int32 LoginIdMax = 20;
	constexpr int32 PasswordMin = 8;
	constexpr int32 PasswordMaxUtf8Bytes = 72;	// BCrypt 한도
	constexpr int32 NicknameMin = 2;
	constexpr int32 NicknameMax = 20;
	constexpr int32 EmailMax = 254;

	bool IsAsciiLetterOrDigit(TCHAR InChar)
	{
		return (InChar >= TEXT('a') && InChar <= TEXT('z'))
			|| (InChar >= TEXT('A') && InChar <= TEXT('Z'))
			|| (InChar >= TEXT('0') && InChar <= TEXT('9'));
	}

	//완성형 한글(가~힣)
	bool IsHangulSyllable(TCHAR InChar)
	{
		return InChar >= 0xAC00 && InChar <= 0xD7A3;
	}
}

UWarriorAuthSubsystem* UWarriorAuthSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorAuthSubsystem>() : nullptr;
}

void UWarriorAuthSubsystem::Deinitialize()
{
	//코어 티커는 이 서브시스템보다 오래 살므로 반드시 뗀다. 서버 세션은 수명이 지나면 사라진다
	StopHeartbeat();

	Super::Deinitialize();
}

//~ Begin 공통 HTTP

bool UWarriorAuthSubsystem::SendPost(const FString& InPath, const FString& InJsonBody, bool bWithAuth, TFunction<void(int32, const FString&)>&& OnDone)
{
	return SendRequest(TEXT("POST"), InPath, InJsonBody, bWithAuth, MoveTemp(OnDone));
}

bool UWarriorAuthSubsystem::SendAuthorized(const FString& InVerb, const FString& InPath, const FString& InJsonBody, TFunction<void(int32, const FString&)>&& OnDone)
{
	if (!bLoggedIn)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Request not sent. Not logged in. %s %s"), *InVerb, *InPath);
		return false;
	}

	//401 처리는 접속 점검과 같은 규칙: 보낼 때의 세션이 아직 이어질 때만 로그인 상태를 끝낸다
	const int32 SentSessionSerial = SessionSerial;
	return SendRequest(InVerb, InPath, InJsonBody, true,
		[this, SentSessionSerial, OnDone = MoveTemp(OnDone)](int32 Status, const FString& Body)
		{
			if (Status == 401 && SentSessionSerial == SessionSerial)
			{
				HandleAuthFailure(Status, ParseApiError(Status, Body));
			}
			OnDone(Status, Body);
		});
}

bool UWarriorAuthSubsystem::SendRequest(const FString& InVerb, const FString& InPath, const FString& InJsonBody, bool bWithAuth, TFunction<void(int32, const FString&)>&& OnDone)
{
	const FString Url = BaseUrl + InPath;

	//토큰 없이 보내면 서버가 401 AUTH_TOKEN_MISSING으로 거절할 뿐이므로 보내지 않는다
	if (bWithAuth && AccessToken.IsEmpty())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Request not sent. No access token. %s %s"), *InVerb, *Url);
		return false;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(InVerb);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetTimeout(RequestTimeoutSeconds);

	//본문이 없는 요청(A4·A5)에는 Content-Type을 붙이지 않는다
	if (!InJsonBody.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(InJsonBody);
	}

	//토큰은 헤더에만 싣고 로그에 남기지 않는다
	if (bWithAuth)
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
	}

	//응답이 오기 전에 게임이 끝나 이 서브시스템이 사라져도 안전하도록 약한 참조로 묶는다
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnDone = MoveTemp(OnDone)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const bool bHasResponse = bConnected && Response.IsValid();
			OnDone(bHasResponse ? Response->GetResponseCode() : 0,
				bHasResponse ? Response->GetContentAsString() : FString());
		});

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Request could not be started. %s %s"), *InVerb, *Url);
		return false;
	}

	return true;
}

FWarriorApiError UWarriorAuthSubsystem::ParseApiError(int32 InStatus, const FString& InBody)
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

FText UWarriorAuthSubsystem::CommonErrorToText(const FString& InCode, int32 InStatus)
{
	if (InCode == TEXT("SERVICE_UNAVAILABLE") || InCode == TEXT("INTERNAL_ERROR") || InStatus >= 500)
	{
		return LOCTEXT("ServerBusy", "서버가 잠시 응답하지 않습니다. 잠시 뒤 다시 시도해 주세요.");
	}
	if (InCode == TEXT("NETWORK_ERROR"))
	{
		return LOCTEXT("Network", "서버에 연결할 수 없습니다. 잠시 뒤 다시 시도해 주세요.");
	}

	return FText::GetEmpty();
}

//~ End 공통 HTTP

//~ Begin 회원가입

void UWarriorAuthSubsystem::RequestSignup(const FString& InLoginId, const FString& InPassword, const FString& InNickname, const FString& InEmail)
{
	if (bRequestInFlight)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Signup ignored. Another request is in flight."));
		return;
	}

	LastSignupFieldErrors.Reset();

	//요청 본문은 명세 칸만. 이메일은 비어 있으면 칸을 뺀다(빈 문자열은 서버가 형식 오류로 볼 수 있다)
	FWarriorSignupRequestDto RequestDto;
	RequestDto.LoginId = InLoginId;
	RequestDto.Password = InPassword;
	RequestDto.Nickname = InNickname;
	if (!InEmail.IsEmpty())
	{
		RequestDto.Email = InEmail;
	}

	FString RequestBody;
	if (!FJsonObjectConverter::UStructToJsonObjectString(RequestDto, RequestBody, 0, 0, 0, nullptr, false))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Signup request body could not be built."));
		FWarriorApiError Error;
		Error.Code = TEXT("INVALID_REQUEST_BODY");
		OnSignupCompleted.Broadcast(false, Error.Code, SignupErrorToText(Error, 0));
		return;
	}

	const bool bStarted = SendPost(TEXT("/auth/signup"), RequestBody, false,
		[this](int32 Status, const FString& Body)
		{
			HandleSignupResponse(Status, Body);
		});

	if (!bStarted)
	{
		FWarriorApiError Error;
		Error.Code = TEXT("NETWORK_ERROR");
		OnSignupCompleted.Broadcast(false, Error.Code, SignupErrorToText(Error, 0));
		return;
	}

	bRequestInFlight = true;
	//아이디·비밀번호·이메일 값과 요청 본문은 로그에 남기지 않는다(로그 파일은 디스크에 남는다)
	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Signup requested. POST %s/auth/signup email=%s"),
		*BaseUrl, InEmail.IsEmpty() ? TEXT("(none)") : TEXT("(given)"));
}

void UWarriorAuthSubsystem::HandleSignupResponse(int32 Status, const FString& Body)
{
	bRequestInFlight = false;

	//── 201: { "data": { accountId, loginId, nickname, createdAt }, "meta": { requestId } }
	//성공 여부는 상태 코드로 정한다. 본문을 못 읽어도 계정은 만들어졌으므로 성공으로 처리한다(본문은 로그용)
	if (Status == 201)
	{
		FWarriorSignupResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Signup 201 but the body could not be read."));
		}

		//로그인 화면이 안내만 하고 아이디 칸은 비워 둔다(아이디는 기억하지 않는다)
		bRecentSignup = true;

		UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Signup OK. accountId=%s requestId=%s"),
			*ResponseDto.Data.AccountId, *ResponseDto.Meta.RequestId);

		OnSignupCompleted.Broadcast(true, FString(), FText::GetEmpty());
		return;
	}

	//── 실패: 400 VALIDATION_FAILED·INVALID_REQUEST_BODY, 409 ACCOUNT_*_DUPLICATED, 503, 연결 실패
	const FWarriorApiError Error = ParseApiError(Status, Body);

	//칸별 오류: 명세는 해당 입력 칸 아래에 표시하도록 한다. 409는 errors[]가 없으므로 코드로 칸을 정한다
	if (Error.Code == TEXT("ACCOUNT_LOGIN_ID_DUPLICATED"))
	{
		LastSignupFieldErrors.Add(TEXT("loginId"), SignupErrorToText(Error, Status));
	}
	else if (Error.Code == TEXT("ACCOUNT_NICKNAME_DUPLICATED"))
	{
		LastSignupFieldErrors.Add(TEXT("nickname"), SignupErrorToText(Error, Status));
	}
	else if (Error.Code == TEXT("VALIDATION_FAILED"))
	{
		//같은 칸에 오류가 여러 개면 줄을 바꿔 붙인다
		for (const FWarriorApiFieldError& FieldError : Error.Errors)
		{
			if (FieldError.Field.IsEmpty() || FieldError.Message.IsEmpty())
			{
				continue;
			}

			if (FText* Existing = LastSignupFieldErrors.Find(FieldError.Field))
			{
				*Existing = FText::FromString(Existing->ToString() + TEXT("\n") + FieldError.Message);
			}
			else
			{
				LastSignupFieldErrors.Add(FieldError.Field, FText::FromString(FieldError.Message));
			}
		}
	}

	if (Error.Code == TEXT("INVALID_REQUEST_BODY"))
	{
		//요청 모양이 명세와 다르다는 뜻이므로 게임 쪽 버그다
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Signup rejected: INVALID_REQUEST_BODY. Check FWarriorSignupRequestDto against the spec."));
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Signup failed. status=%d code=%s retryable=%s fieldErrors=%d"),
			Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"), Error.Errors.Num());
	}

	OnSignupCompleted.Broadcast(false, Error.Code, SignupErrorToText(Error, Status));
}

FText UWarriorAuthSubsystem::SignupErrorToText(const FWarriorApiError& InError, int32 InStatus)
{
	if (InError.Code == TEXT("ACCOUNT_LOGIN_ID_DUPLICATED"))
	{
		return LOCTEXT("SignupLoginIdDuplicated", "이미 사용 중인 아이디입니다.");
	}
	if (InError.Code == TEXT("ACCOUNT_NICKNAME_DUPLICATED"))
	{
		return LOCTEXT("SignupNicknameDuplicated", "이미 사용 중인 닉네임입니다.");
	}
	if (InError.Code == TEXT("VALIDATION_FAILED"))
	{
		//명세 제안: 칸별 오류(errors[].message)를 보여 준다. 지금은 문구 칸이 하나라 줄을 바꿔 모두 보여 준다
		TArray<FString> Lines;
		for (const FWarriorApiFieldError& FieldError : InError.Errors)
		{
			if (!FieldError.Message.IsEmpty())
			{
				Lines.Add(FieldError.Message);
			}
		}
		return Lines.Num() > 0
			? FText::FromString(FString::Join(Lines, TEXT("\n")))
			: LOCTEXT("SignupValidation", "입력한 값을 다시 확인해 주세요.");
	}

	const FText CommonText = CommonErrorToText(InError.Code, InStatus);
	if (!CommonText.IsEmpty())
	{
		return CommonText;
	}

	//INVALID_REQUEST_BODY, 모르는 코드
	return LOCTEXT("SignupUnknown", "가입하지 못했습니다. 잠시 뒤 다시 시도해 주세요.");
}

//~ End 회원가입

//~ Begin 로그인

void UWarriorAuthSubsystem::RequestLogin(const FString& InLoginId, const FString& InPassword)
{
	if (bRequestInFlight)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Login ignored. Another request is in flight."));
		return;
	}

	//요청 본문은 명세 칸(loginId, password)만. 모르는 칸이 있으면 서버가 400 INVALID_REQUEST_BODY로 거절한다
	FWarriorLoginRequestDto RequestDto;
	RequestDto.LoginId = InLoginId;
	RequestDto.Password = InPassword;

	FString RequestBody;
	if (!FJsonObjectConverter::UStructToJsonObjectString(RequestDto, RequestBody, 0, 0, 0, nullptr, false))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Login request body could not be built."));
		OnLoginCompleted.Broadcast(false, TEXT("INVALID_REQUEST_BODY"), LoginErrorToText(TEXT("INVALID_REQUEST_BODY"), 0));
		return;
	}

	const bool bStarted = SendPost(TEXT("/auth/login"), RequestBody, false,
		[this, InLoginId](int32 Status, const FString& Body)
		{
			HandleLoginResponse(InLoginId, Status, Body);
		});

	if (!bStarted)
	{
		OnLoginCompleted.Broadcast(false, TEXT("NETWORK_ERROR"), LoginErrorToText(TEXT("NETWORK_ERROR"), 0));
		return;
	}

	bRequestInFlight = true;
	//아이디·비밀번호와 요청 본문은 로그에 남기지 않는다(로그 파일은 디스크에 남는다)
	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Login requested. POST %s/auth/login"), *BaseUrl);
}

void UWarriorAuthSubsystem::HandleLoginResponse(const FString& InLoginId, int32 Status, const FString& Body)
{
	bRequestInFlight = false;
	LastLoginRetryAfterSeconds = 0;

	//── 200: { "data": { accessToken, tokenType, sessionExpiresAt, account }, "meta": { requestId } }
	if (Status == 200)
	{
		FWarriorLoginResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto) || ResponseDto.Data.AccessToken.IsEmpty())
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Login 200 but the body could not be read."));
			OnLoginCompleted.Broadcast(false, TEXT("INVALID_RESPONSE"), LoginErrorToText(TEXT("INVALID_RESPONSE"), Status));
			return;
		}

		const FWarriorLoginResultDto& Result = ResponseDto.Data;

		//토큰은 메모리에만 둔다(로그 금지). 앱을 다시 켜면 다시 로그인한다
		AccessToken = Result.AccessToken;
		if (!FDateTime::ParseIso8601(*Result.SessionExpiresAt, SessionExpiresAt))
		{
			SessionExpiresAt = FDateTime();
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Login sessionExpiresAt could not be parsed: %s"), *Result.SessionExpiresAt);
		}

		//로그인 응답에는 loginId·nickname이 없으므로 아이디는 요청 값을 쓴다
		Account = FWarriorAuthAccount();
		Account.AccountId = Result.Account.AccountId;
		Account.LoginId = InLoginId;

		//명세 순서: 메인화면 값을 먼저 적용하고 그다음 메인메뉴로 간다(성공 알림을 먼저 보내면 메인메뉴가 예전 값을 잠깐 보인다)
		ApplyAccountSnapshot(Result.Account);
		bLoggedIn = true;

		//새로 로그인했으므로 이전 로그인이 끝난 이유는 더 보여 줄 필요가 없다
		PendingSessionEndReason = EWarriorSessionEndReason::None;
		PendingSessionEndMessage = FText::GetEmpty();

		++SessionSerial;
		StartHeartbeat();

		UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Login OK. accountId=%s level=%d requestId=%s"),
			*Account.AccountId, Result.Account.AccountLevel, *ResponseDto.Meta.RequestId);

		OnLoginCompleted.Broadcast(true, FString(), FText::GetEmpty());
		return;
	}

	//── 실패: { code, message, path, retryable, errors, retryAfterSeconds? } / 연결 실패는 본문 없음
	const FWarriorApiError Error = ParseApiError(Status, Body);

	//429 잠김: 남은 초를 기억해 로그인 화면이 그동안 버튼을 막게 한다
	if (Error.Code == TEXT("AUTH_LOGIN_LOCKED") || Status == 429)
	{
		LastLoginRetryAfterSeconds = FMath::Max(0, Error.RetryAfterSeconds);
	}

	if (Error.Code == TEXT("INVALID_REQUEST_BODY"))
	{
		//요청 모양이 명세와 다르다는 뜻이므로 게임 쪽 버그다
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Login rejected: INVALID_REQUEST_BODY. Check FWarriorLoginRequestDto against the spec."));
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Login failed. status=%d code=%s retryable=%s retryAfter=%d"),
			Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"), LastLoginRetryAfterSeconds);
	}

	OnLoginCompleted.Broadcast(false, Error.Code, LoginErrorToText(Error.Code, Status, LastLoginRetryAfterSeconds));
}

void UWarriorAuthSubsystem::ApplyAccountSnapshot(const FWarriorAccountSnapshotDto& InSnapshot)
{
	if (InSnapshot.Version < LastAppliedAccountVersion)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Account snapshot version %lld is older than %lld. Ignored."),
			InSnapshot.Version, LastAppliedAccountVersion);
		return;
	}

	UWarriorAccountSubsystem* AccountSubsystem = GetGameInstance()->GetSubsystem<UWarriorAccountSubsystem>();
	if (!AccountSubsystem)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] No account subsystem. Account snapshot is not applied."));
		return;
	}

	//ApplyServerSnapshot은 통째로 덮어쓰므로, 지금 값에서 시작해 스냅샷에 있는 칸만 바꾼다
	//(RewardedRecordIds·ExternalCurrencies·TotalExperience는 스냅샷에 없어 지금 값을 유지한다)
	FWarriorAccountData Data = AccountSubsystem->GetAccountData();
	Data.AccountLevel = InSnapshot.AccountLevel;
	Data.Experience = InSnapshot.Experience;
	Data.StatPoints = InSnapshot.StatPoints;

	//태그는 문자열로 온다. 모르는 태그는 건너뛴다(ErrorIfNotFound = false)
	Data.InvestedStats.Reset();
	for (const TPair<FString, int32>& Pair : InSnapshot.InvestedStats)
	{
		const FGameplayTag StatTag = FGameplayTag::RequestGameplayTag(FName(*Pair.Key), false);
		if (StatTag.IsValid())
		{
			Data.InvestedStats.Add(StatTag, Pair.Value);
		}
		else
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Unknown stat tag in snapshot: %s"), *Pair.Key);
		}
	}

	Data.UnlockedSkills.Reset();
	for (const FString& SkillName : InSnapshot.UnlockedSkills)
	{
		const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(FName(*SkillName), false);
		if (SkillTag.IsValid())
		{
			Data.UnlockedSkills.AddTag(SkillTag);
		}
		else
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Unknown skill tag in snapshot: %s"), *SkillName);
		}
	}

	LastAppliedAccountVersion = InSnapshot.Version;
	AccountSubsystem->ApplyServerSnapshot(Data);
}

FText UWarriorAuthSubsystem::LoginErrorToText(const FString& InCode, int32 InStatus, int32 InRetryAfterSeconds)
{
	if (InCode == TEXT("AUTH_INVALID_CREDENTIALS") || InStatus == 401)
	{
		return LOCTEXT("LoginInvalidCredentials", "아이디 또는 비밀번호가 맞지 않습니다.");
	}
	if (InCode == TEXT("AUTH_LOGIN_LOCKED") || InStatus == 429)
	{
		return InRetryAfterSeconds > 0
			? FText::Format(LOCTEXT("LoginLockedSeconds", "로그인 시도가 너무 많습니다. {0}초 뒤 다시 시도해 주세요."), InRetryAfterSeconds)
			: LOCTEXT("LoginLocked", "로그인 시도가 너무 많습니다. 잠시 뒤 다시 시도해 주세요.");
	}
	if (InCode == TEXT("ACCOUNT_SUSPENDED") || InStatus == 403)
	{
		return LOCTEXT("LoginSuspended", "이용이 제한된 계정입니다.");
	}
	if (InCode == TEXT("VALIDATION_FAILED"))
	{
		return LOCTEXT("LoginValidation", "아이디와 비밀번호를 입력해 주세요.");
	}

	const FText CommonText = CommonErrorToText(InCode, InStatus);
	if (!CommonText.IsEmpty())
	{
		return CommonText;
	}

	//INVALID_REQUEST_BODY, INVALID_RESPONSE, 모르는 코드
	return LOCTEXT("LoginUnknown", "로그인하지 못했습니다. 잠시 뒤 다시 시도해 주세요.");
}

//~ End 로그인

//~ Begin 세션 종료

bool UWarriorAuthSubsystem::HandleAuthFailure(int32 InStatus, const FWarriorApiError& InError)
{
	if (InStatus != 401)
	{
		return false;
	}

	//명세 인증 절: 401 코드 네 개. 앞에서 걸리면 뒤는 보지 않으므로 한 번에 하나만 온다
	EWarriorSessionEndReason Reason = EWarriorSessionEndReason::Expired;
	if (InError.Code == TEXT("AUTH_SESSION_NOT_FOUND"))
	{
		Reason = EWarriorSessionEndReason::Expired;
	}
	else if (InError.Code == TEXT("AUTH_SESSION_REPLACED"))
	{
		Reason = EWarriorSessionEndReason::Replaced;
	}
	else if (InError.Code == TEXT("AUTH_TOKEN_MISSING") || InError.Code == TEXT("AUTH_TOKEN_INVALID"))
	{
		//토큰을 빠뜨렸거나 모양이 틀렸다는 뜻이므로 게임 쪽 버그다
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] %s. Check the Authorization header in SendPost."), *InError.Code);
		Reason = EWarriorSessionEndReason::InvalidToken;
	}
	else
	{
		//모르는 코드(본문을 못 읽은 HTTP_401 포함). 세션을 믿을 수 없으므로 다시 로그인하게 한다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Unknown 401 code %s. Treated as an expired session."), *InError.Code);
	}

	EndSession(Reason);
	return true;
}

void UWarriorAuthSubsystem::EndSession(EWarriorSessionEndReason InReason)
{
	//접속 점검과 다른 요청이 거의 동시에 401을 받아도 처리는 한 번만 한다
	if (!bLoggedIn)
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Auth] EndSession(%s) ignored. Not logged in."), *UEnum::GetValueAsString(InReason));
		return;
	}

	StopHeartbeat();
	bRealtimeConnected = false;

	//로그인 때 채운 값을 모두 되돌린다. 토큰은 로그에 남기지 않는다
	AccessToken.Reset();
	SessionExpiresAt = FDateTime();
	Account = FWarriorAuthAccount();
	bLoggedIn = false;

	//스냅샷 번호도 되돌린다. 그대로 두면 다른 계정으로 로그인했을 때 그 계정의 번호가 더 작아 스냅샷이 무시된다
	LastAppliedAccountVersion = 0;

	//알림을 받을 화면이 아직 없을 수 있으므로(레벨 이동 중) 이유를 남겨 둔다
	PendingSessionEndReason = InReason;
	PendingSessionEndMessage = SessionEndToText(InReason);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Session ended. reason=%s"), *UEnum::GetValueAsString(InReason));

	OnSessionEnded.Broadcast(InReason, PendingSessionEndMessage);
}

bool UWarriorAuthSubsystem::ConsumePendingSessionEnd(EWarriorSessionEndReason& OutReason, FText& OutMessage)
{
	if (PendingSessionEndReason == EWarriorSessionEndReason::None)
	{
		return false;
	}

	OutReason = PendingSessionEndReason;
	OutMessage = PendingSessionEndMessage;
	PendingSessionEndReason = EWarriorSessionEndReason::None;
	PendingSessionEndMessage = FText::GetEmpty();
	return true;
}

FText UWarriorAuthSubsystem::SessionEndToText(EWarriorSessionEndReason InReason)
{
	switch (InReason)
	{
	case EWarriorSessionEndReason::Expired:
		//만료·제재로 끊김이 같은 코드로 오므로 둘 다에 맞는 문장을 쓴다
		return LOCTEXT("SessionExpired", "접속이 종료되었습니다. 다시 로그인해 주세요.");
	case EWarriorSessionEndReason::Replaced:
		return LOCTEXT("SessionReplaced", "다른 곳에서 같은 계정으로 로그인하여 접속이 종료되었습니다.");
	case EWarriorSessionEndReason::InvalidToken:
		return LOCTEXT("SessionInvalidToken", "로그인 정보가 올바르지 않습니다. 다시 로그인해 주세요.");
	case EWarriorSessionEndReason::ConnectionLost:
		return LOCTEXT("SessionConnectionLost", "서버와 연결이 끊어졌습니다. 다시 로그인해 주세요.");
	default:
		//LoggedOut은 플레이어가 직접 한 일이라 안내하지 않는다
		return FText::GetEmpty();
	}
}

//~ End 세션 종료

//~ Begin 로그아웃

void UWarriorAuthSubsystem::RequestLogout()
{
	if (!bLoggedIn)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Logout ignored. Not logged in."));
		return;
	}

	//요청을 먼저 보낸다. EndSession이 토큰을 지우므로 순서가 바뀌면 인증 헤더 없이 나간다
	const bool bStarted = SendPost(TEXT("/auth/logout"), FString(), true,
		[this](int32 Status, const FString& Body)
		{
			HandleLogoutResponse(Status, Body);
		});

	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Logout requested. POST %s/auth/logout started=%s"),
		*BaseUrl, bStarted ? TEXT("true") : TEXT("false"));

	//명세: 응답을 받든 못 받든 토큰을 지우고 타이틀로 간다. 그래서 응답을 기다리지 않는다
	EndSession(EWarriorSessionEndReason::LoggedOut);
}

void UWarriorAuthSubsystem::HandleLogoutResponse(int32 Status, const FString& Body)
{
	//── 204: 본문 없음. 서버 세션을 지웠다
	if (Status == 204)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Logout OK."));
		return;
	}

	//── 실패: 이미 로그아웃 처리를 마쳤으므로 화면은 바꾸지 않는다.
	//401은 HandleAuthFailure로 보내지 않는다(로그아웃한 플레이어에게 만료·중복 로그인 안내를 띄우지 않기 위해).
	//401 AUTH_SESSION_NOT_FOUND: 서버 세션이 이미 없었다(만료). 401 AUTH_SESSION_REPLACED: 다른 곳의 새 세션은 지워지지 않는다.
	//연결 실패·5xx: 서버 세션은 수명(10분)이 지나면 사라진다
	const FWarriorApiError Error = ParseApiError(Status, Body);
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Logout response was not 204. status=%d code=%s (already logged out locally)"),
		Status, *Error.Code);
}

//~ End 로그아웃

//~ Begin 접속 점검

void UWarriorAuthSubsystem::StartHeartbeat()
{
	StopHeartbeat();

	HeartbeatFailureCount = 0;
	bHeartbeatInFlight = false;

	//일시 정지·레벨 이동 중에도 실제 시간으로 돌아야 서버 세션이 끊기지 않으므로 월드 타이머 대신 코어 티커를 쓴다
	const float Interval = FMath::Max(1.f, HeartbeatIntervalSeconds);
	HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleHeartbeatTick), Interval);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Heartbeat started. interval=%.0fs"), Interval);
}

void UWarriorAuthSubsystem::StopHeartbeat()
{
	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
		UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Heartbeat stopped."));
	}
}

bool UWarriorAuthSubsystem::HandleHeartbeatTick(float InDeltaTime)
{
	SendHeartbeat();
	return true;
}

void UWarriorAuthSubsystem::SendHeartbeat()
{
	if (!bLoggedIn)
	{
		return;
	}

	//응답이 늦으면(최대 RequestTimeoutSeconds) 다음 차례를 건너뛴다. 겹쳐 보내도 세션은 더 늘지 않는다
	if (bHeartbeatInFlight)
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Auth] Heartbeat skipped. Previous one is still in flight."));
		return;
	}

	const int32 SentSessionSerial = SessionSerial;
	const bool bStarted = SendPost(TEXT("/auth/heartbeat"), FString(), true,
		[this, SentSessionSerial](int32 Status, const FString& Body)
		{
			HandleHeartbeatResponse(SentSessionSerial, Status, Body);
		});

	if (bStarted)
	{
		bHeartbeatInFlight = true;
		return;
	}

	//요청을 시작하지 못한 것도 연결 실패로 센다
	HandleHeartbeatResponse(SentSessionSerial, 0, FString());
}

void UWarriorAuthSubsystem::HandleHeartbeatResponse(int32 InSessionSerial, int32 Status, const FString& Body)
{
	//지난 세션의 응답(로그아웃 뒤 다시 로그인한 경우 등)은 새 세션에 영향을 주지 않는다
	if (InSessionSerial != SessionSerial || !bLoggedIn)
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Auth] Heartbeat response for an old session ignored. status=%d"), Status);
		return;
	}

	bHeartbeatInFlight = false;

	//── 200: { "data": { sessionExpiresAt }, "meta": { requestId } }. 세션은 인증 확인에서 이미 10분 늘었다
	if (Status == 200)
	{
		HeartbeatFailureCount = 0;

		FWarriorHeartbeatResponseDto ResponseDto;
		if (FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto)
			&& !FDateTime::ParseIso8601(*ResponseDto.Data.SessionExpiresAt, SessionExpiresAt))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Heartbeat sessionExpiresAt could not be parsed: %s"), *ResponseDto.Data.SessionExpiresAt);
		}

		UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Heartbeat OK. sessionExpiresAt=%s requestId=%s"),
			*ResponseDto.Data.SessionExpiresAt, *ResponseDto.Meta.RequestId);
		return;
	}

	const FWarriorApiError Error = ParseApiError(Status, Body);

	//── 401: 만료·중복 로그인 등. 다시 시도해도 소용없으므로 바로 끝낸다
	if (HandleAuthFailure(Status, Error))
	{
		return;
	}

	//── 연결 실패·503·그 밖: 로그인은 끝내지 않고 다음 차례에 다시 본다(realtime-api.md v1.1 "다시 연결").
	//서버가 돌아오면 세션이 살아 있는 한 그대로 이어지고, 10분 넘게 끊겼다면 다음 점검이 401이 되어 그때 끝난다
	++HeartbeatFailureCount;
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Heartbeat failed (%d in a row). status=%d code=%s. Keeping the session and retrying."),
		HeartbeatFailureCount, Status, *Error.Code);
}

void UWarriorAuthSubsystem::CheckSessionNow()
{
	SendHeartbeat();
}

//~ End 접속 점검

//~ Begin 실시간 연결

TSharedPtr<IWebSocket> UWarriorAuthSubsystem::CreateRealtimeSocket(const FString& InPath) const
{
	if (!bLoggedIn || AccessToken.IsEmpty())
	{
		return nullptr;
	}

	//http(s)://host → ws(s)://host. 토큰은 주소가 아니라 헤더에만 싣는다(주소는 로그에 남을 수 있다)
	FString Url = BaseUrl;
	if (Url.StartsWith(TEXT("https://")))
	{
		Url = TEXT("wss://") + Url.RightChop(8);
	}
	else if (Url.StartsWith(TEXT("http://")))
	{
		Url = TEXT("ws://") + Url.RightChop(7);
	}
	Url += InPath;

	TMap<FString, FString> UpgradeHeaders;
	UpgradeHeaders.Add(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);

	return FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets")).CreateWebSocket(Url, FString(), UpgradeHeaders);
}

void UWarriorAuthSubsystem::EndSessionByServer(EWarriorSessionEndReason InReason)
{
	EndSession(InReason);
}

void UWarriorAuthSubsystem::SetRealtimeConnected(bool bConnected)
{
	if (bRealtimeConnected == bConnected)
	{
		return;
	}
	bRealtimeConnected = bConnected;

	if (!bPauseHeartbeatWhileRealtimeConnected || !bLoggedIn)
	{
		return;
	}

	//연결이 붙어 있으면 Ping/Pong이 세션을 늘리므로 접속 점검을 쉬고, 끊기면 다시 돌린다
	if (bConnected)
	{
		StopHeartbeat();
	}
	else
	{
		StartHeartbeat();
	}
}

//~ End 실시간 연결

bool UWarriorAuthSubsystem::ConsumeRecentSignup()
{
	const bool bWasRecentSignup = bRecentSignup;
	bRecentSignup = false;
	return bWasRecentSignup;
}

FText UWarriorAuthSubsystem::ValidateLoginId(const FString& InLoginId)
{
	if (InLoginId.Len() < WarriorAuthRules::LoginIdMin || InLoginId.Len() > WarriorAuthRules::LoginIdMax)
	{
		return FText::Format(LOCTEXT("LoginIdLength", "아이디는 {0}~{1}자로 입력해 주세요."), WarriorAuthRules::LoginIdMin, WarriorAuthRules::LoginIdMax);
	}

	//영문 대소문자·숫자. 대소문자를 구분하므로 바꾸지 않고 그대로 보낸다
	for (const TCHAR Char : InLoginId)
	{
		if (!WarriorAuthRules::IsAsciiLetterOrDigit(Char))
		{
			return LOCTEXT("LoginIdChars", "아이디는 영문과 숫자만 쓸 수 있습니다.");
		}
	}

	return FText::GetEmpty();
}

FText UWarriorAuthSubsystem::ValidatePassword(const FString& InPassword)
{
	if (InPassword.Len() < WarriorAuthRules::PasswordMin)
	{
		return FText::Format(LOCTEXT("PasswordTooShort", "비밀번호는 {0}자 이상 입력해 주세요."), WarriorAuthRules::PasswordMin);
	}

	//BCrypt 한도: UTF-8로 72바이트까지(영문·숫자 1바이트, 한글 3바이트 → 한글만이면 24자)
	const int32 Utf8Bytes = FTCHARToUTF8(*InPassword).Length();
	if (Utf8Bytes > WarriorAuthRules::PasswordMaxUtf8Bytes)
	{
		return LOCTEXT("PasswordTooLong", "비밀번호가 너무 깁니다. 영문·숫자는 72자, 한글은 24자까지 쓸 수 있습니다.");
	}

	return FText::GetEmpty();
}

FText UWarriorAuthSubsystem::ValidateNickname(const FString& InNickname)
{
	if (InNickname.TrimStartAndEnd().IsEmpty())
	{
		return LOCTEXT("NicknameBlank", "닉네임을 입력해 주세요.");
	}

	//앞뒤 공백은 안 된다(화면은 보내기 전에 앞뒤 공백을 지운다)
	if (FChar::IsWhitespace(InNickname[0]) || FChar::IsWhitespace(InNickname[InNickname.Len() - 1]))
	{
		return LOCTEXT("NicknameEdgeSpace", "닉네임 앞뒤에는 공백을 쓸 수 없습니다.");
	}

	if (InNickname.Len() < WarriorAuthRules::NicknameMin || InNickname.Len() > WarriorAuthRules::NicknameMax)
	{
		return FText::Format(LOCTEXT("NicknameLength", "닉네임은 {0}~{1}자로 입력해 주세요."), WarriorAuthRules::NicknameMin, WarriorAuthRules::NicknameMax);
	}

	//한글·영문·숫자, 가운데 공백
	for (const TCHAR Char : InNickname)
	{
		if (!(WarriorAuthRules::IsAsciiLetterOrDigit(Char) || WarriorAuthRules::IsHangulSyllable(Char) || Char == TEXT(' ')))
		{
			return LOCTEXT("NicknameChars", "닉네임은 한글·영문·숫자만 쓸 수 있습니다.");
		}
	}

	return FText::GetEmpty();
}

FText UWarriorAuthSubsystem::ValidateEmail(const FString& InEmail)
{
	if (InEmail.IsEmpty())
	{
		return FText::GetEmpty();
	}

	//정확한 검증은 서버가 한다. 여기서는 "a@b.c" 모양인지만 본다
	FString Local;
	FString Domain;
	const bool bShapeOk = InEmail.Len() <= WarriorAuthRules::EmailMax
		&& !InEmail.Contains(TEXT(" "))
		&& InEmail.Split(TEXT("@"), &Local, &Domain)
		&& !Local.IsEmpty()
		&& !Domain.Contains(TEXT("@"))
		&& Domain.Contains(TEXT("."))
		&& !Domain.StartsWith(TEXT("."))
		&& !Domain.EndsWith(TEXT("."));

	return bShapeOk ? FText::GetEmpty() : LOCTEXT("EmailInvalid", "이메일 형식이 맞지 않습니다. 비워 두어도 됩니다.");
}