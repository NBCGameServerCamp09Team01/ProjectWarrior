// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAuthSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"

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

	//TODO(server): POST /api/v1/auth/signup 으로 교체한다. 본문 { loginId, password, nickname, email? } — email이 비면 빼거나 null.
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

	bRequestInFlight = true;
	UE_LOG(LogProjectWarrior, Log, TEXT("[Auth] Login requested. loginId=%s"), *InLoginId);

	//TODO(server): POST /api/v1/auth/login 으로 교체한다. 200이면 accessToken·expiresAt·account 저장, 401은 AUTH_INVALID_CREDENTIALS
	GetGameInstance()->GetTimerManager().SetTimer(PlaceholderTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::CompleteLoginPlaceholder, InLoginId),
		WarriorAuthRules::PlaceholderDelay, false);
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

void UWarriorAuthSubsystem::CompleteLoginPlaceholder(FString InLoginId)
{
	bRequestInFlight = false;
	bLoggedIn = true;

	Account = FWarriorAuthAccount();
	Account.LoginId = InLoginId;
	Account.Nickname = InLoginId;
	Account.CreatedAt = FDateTime::UtcNow();

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] Login succeeded with PLACEHOLDER response (server not connected). loginId=%s"), *InLoginId);
	OnLoginCompleted.Broadcast(true, FString(), FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
