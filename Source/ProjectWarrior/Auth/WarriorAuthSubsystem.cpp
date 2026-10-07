// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAuthSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTagContainer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"

#define LOCTEXT_NAMESPACE "WarriorAuth"

namespace WarriorAuthRules
{
	//docs/contracts/auth-api.md 회원가입 요청 표의 제안 값. 서버와 확정되면 같이 고친다
	constexpr int32 LoginIdMin = 4;
	constexpr int32 LoginIdMax = 20;
	constexpr int32 PasswordMin = 8;
	constexpr int32 PasswordMax = 64;
	constexpr int32 NicknameMin = 2;
	constexpr int32 NicknameMax = 12;
	constexpr int32 EmailMax = 254;

	//TODO(server): 임시 응답 지연(초)
	constexpr float PlaceholderDelay = 0.4f;
}

UWarriorAuthSubsystem* UWarriorAuthSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorAuthSubsystem>() : nullptr;
}

void UWarriorAuthSubsystem::Deinitialize()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(PlaceholderTimer);
	}

	Super::Deinitialize();
}

void UWarriorAuthSubsystem::RequestSignup(const FString& InLoginId, const FString& InPassword, const FString& InNickname, const FString& InEmail)
{
	if (bRequestInFlight)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Signup ignored. Another request is in flight."));
		return;
	}

	bRequestInFlight = true;
	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Signup requested. loginId=%s, email=%s"), *InLoginId, InEmail.IsEmpty() ? TEXT("(none)") : TEXT("(given)"));

	//TODO(server): POST {BaseUrl}/auth/signup 으로 교체한다. 본문 { loginId, password, nickname, email? } — email이 비면 빼거나 null.
	//201이면 성공, 409는 아이디·닉네임 중복(서버 오류 코드로 구분)
	GetGameInstance()->GetTimerManager().SetTimer(PlaceholderTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::CompleteSignupPlaceholder, InLoginId),
		WarriorAuthRules::PlaceholderDelay, false);
}

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

	const FString Url = BaseUrl + TEXT("/auth/login");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetContentAsString(RequestBody);
	Request->SetTimeout(RequestTimeoutSeconds);

	//응답이 오기 전에 게임이 끝나 이 서브시스템이 사라져도 안전하도록 약한 참조로 묶는다
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, InLoginId](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const bool bHasResponse = bConnected && Response.IsValid();
			HandleLoginResponse(InLoginId,
				bHasResponse ? Response->GetResponseCode() : 0,
				bHasResponse ? Response->GetContentAsString() : FString());
		});

	bRequestInFlight = true;
	//비밀번호와 요청 본문은 로그에 남기지 않는다
	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Login requested. POST %s loginId=%s"), *Url, *InLoginId);

	if (!Request->ProcessRequest())
	{
		bRequestInFlight = false;
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Login request could not be started. url=%s"), *Url);
		OnLoginCompleted.Broadcast(false, TEXT("NETWORK_ERROR"), LoginErrorToText(TEXT("NETWORK_ERROR"), 0));
	}
}

void UWarriorAuthSubsystem::HandleLoginResponse(const FString& InLoginId, int32 Status, const FString& Body)
{
	bRequestInFlight = false;

	//── 200: { "data": { accessToken, tokenType, expiresAt, account }, "meta": { requestId } }
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
		if (!FDateTime::ParseIso8601(*Result.ExpiresAt, AccessTokenExpiresAt))
		{
			AccessTokenExpiresAt = FDateTime();
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Login expiresAt could not be parsed: %s"), *Result.ExpiresAt);
		}

		//로그인 응답에는 loginId·nickname이 없으므로 아이디는 요청 값을 쓴다
		Account = FWarriorAuthAccount();
		Account.AccountId = Result.Account.AccountId;
		Account.LoginId = InLoginId;

		//명세 순서: 메인화면 값을 먼저 적용하고 그다음 메인메뉴로 간다(성공 알림을 먼저 보내면 메인메뉴가 예전 값을 잠깐 보인다)
		ApplyAccountSnapshot(Result.Account);
		bLoggedIn = true;

		UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Login OK. accountId=%s level=%d requestId=%s"),
			*Account.AccountId, Result.Account.Level, *ResponseDto.Meta.RequestId);

		OnLoginCompleted.Broadcast(true, FString(), FText::GetEmpty());
		return;
	}

	//── 실패: { code, message, path, retryable, errors } / 연결 실패는 본문 없음
	FWarriorApiError Error;
	if (Status == 0)
	{
		Error.Code = TEXT("NETWORK_ERROR");
		Error.Retryable = true;
	}
	else if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &Error))
	{
		Error.Code = FString::Printf(TEXT("HTTP_%d"), Status);
	}

	if (Error.Code == TEXT("INVALID_REQUEST_BODY"))
	{
		//요청 모양이 명세와 다르다는 뜻이므로 게임 쪽 버그다
		UE_LOG(LogProjectWarrior, Error, TEXT("[Auth] Login rejected: INVALID_REQUEST_BODY. Check FWarriorLoginRequestDto against the spec."));
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Login failed. status=%d code=%s retryable=%s"),
			Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"));
	}

	OnLoginCompleted.Broadcast(false, Error.Code, LoginErrorToText(Error.Code, Status));
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
	Data.AccountLevel = InSnapshot.Level;
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

FText UWarriorAuthSubsystem::LoginErrorToText(const FString& InCode, int32 InStatus)
{
	if (InCode == TEXT("AUTH_INVALID_CREDENTIALS") || InStatus == 401)
	{
		return LOCTEXT("LoginInvalidCredentials", "아이디 또는 비밀번호가 맞지 않습니다.");
	}
	if (InCode == TEXT("AUTH_LOGIN_LOCKED") || InStatus == 429)
	{
		return LOCTEXT("LoginLocked", "로그인 시도가 너무 많습니다. 잠시 뒤 다시 시도해 주세요.");
	}
	if (InCode == TEXT("VALIDATION_FAILED"))
	{
		return LOCTEXT("LoginValidation", "아이디와 비밀번호를 입력해 주세요.");
	}
	if (InCode == TEXT("SERVICE_UNAVAILABLE") || InCode == TEXT("INTERNAL_ERROR") || InStatus >= 500)
	{
		return LOCTEXT("LoginServerBusy", "서버가 잠시 응답하지 않습니다. 잠시 뒤 다시 시도해 주세요.");
	}
	if (InCode == TEXT("NETWORK_ERROR"))
	{
		return LOCTEXT("LoginNetwork", "서버에 연결할 수 없습니다. 잠시 뒤 다시 시도해 주세요.");
	}

	//INVALID_REQUEST_BODY, INVALID_RESPONSE, 모르는 코드
	return LOCTEXT("LoginUnknown", "로그인하지 못했습니다. 잠시 뒤 다시 시도해 주세요.");
}

bool UWarriorAuthSubsystem::ConsumeRecentSignupLoginId(FString& OutLoginId)
{
	if (RecentSignupLoginId.IsEmpty())
	{
		return false;
	}

	OutLoginId = MoveTemp(RecentSignupLoginId);
	RecentSignupLoginId.Reset();
	return true;
}

FText UWarriorAuthSubsystem::ValidateLoginId(const FString& InLoginId)
{
	if (InLoginId.Len() < WarriorAuthRules::LoginIdMin || InLoginId.Len() > WarriorAuthRules::LoginIdMax)
	{
		return FText::Format(LOCTEXT("LoginIdLength", "아이디는 {0}~{1}자로 입력해 주세요."), WarriorAuthRules::LoginIdMin, WarriorAuthRules::LoginIdMax);
	}

	for (const TCHAR Char : InLoginId)
	{
		if (!((Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9'))))
		{
			return LOCTEXT("LoginIdChars", "아이디는 영문 소문자와 숫자만 쓸 수 있습니다.");
		}
	}

	return FText::GetEmpty();
}

FText UWarriorAuthSubsystem::ValidatePassword(const FString& InPassword)
{
	if (InPassword.Len() < WarriorAuthRules::PasswordMin || InPassword.Len() > WarriorAuthRules::PasswordMax)
	{
		return FText::Format(LOCTEXT("PasswordLength", "비밀번호는 {0}~{1}자로 입력해 주세요."), WarriorAuthRules::PasswordMin, WarriorAuthRules::PasswordMax);
	}

	return FText::GetEmpty();
}

FText UWarriorAuthSubsystem::ValidateNickname(const FString& InNickname)
{
	if (InNickname.TrimStartAndEnd().IsEmpty())
	{
		return LOCTEXT("NicknameBlank", "닉네임을 입력해 주세요.");
	}

	if (InNickname.Len() < WarriorAuthRules::NicknameMin || InNickname.Len() > WarriorAuthRules::NicknameMax)
	{
		return FText::Format(LOCTEXT("NicknameLength", "닉네임은 {0}~{1}자로 입력해 주세요."), WarriorAuthRules::NicknameMin, WarriorAuthRules::NicknameMax);
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

void UWarriorAuthSubsystem::CompleteSignupPlaceholder(FString InLoginId)
{
	bRequestInFlight = false;
	RecentSignupLoginId = InLoginId;

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Signup succeeded with PLACEHOLDER response (server not connected). loginId=%s"), *InLoginId);
	OnSignupCompleted.Broadcast(true, FString(), FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
