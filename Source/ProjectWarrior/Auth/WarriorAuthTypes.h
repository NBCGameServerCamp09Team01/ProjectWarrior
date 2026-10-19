// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorAuthTypes.generated.h"

/**
 * 로그인 상태가 끝난 이유. 화면은 이 값으로 갈 곳(타이틀·로그인 화면)과 안내 팝업을 정한다.
 * BP에 저장된 값이 바뀌지 않도록 새 값은 끝에 추가한다.
 */
UENUM(BlueprintType)
enum class EWarriorSessionEndReason : uint8
{
	None,
	LoggedOut,		// 플레이어가 로그아웃함 → 타이틀(안내 없음)
	Expired,		// 401 AUTH_SESSION_NOT_FOUND. 만료·로그아웃·제재로 끊김(게임은 구분할 수 없다) → 로그인 화면
	Replaced,		// 401 AUTH_SESSION_REPLACED. 다른 곳에서 같은 계정으로 로그인함 → 안내 후 타이틀
	InvalidToken,	// 401 AUTH_TOKEN_MISSING·AUTH_TOKEN_INVALID. 게임 쪽 버그 → 로그인 화면
	ConnectionLost	// 쓰지 않음(realtime-api.md v1.1: 연결이 끊긴 것만으로는 로그인을 끝내지 않는다). BP에 저장된 값이 밀리지 않게 남겨 둔다
};

/**
 * 인증 API의 계정 본문(docs/contracts/auth-api.md "계정 본문").
 * 비밀번호와 토큰은 여기에 두지 않는다(토큰은 UWarriorAuthSubsystem만 보관한다).
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAuthAccount
{
	GENERATED_BODY()

	//서버가 만든 계정 번호. DB에서는 BIGINT지만 JSON에서는 문자열로 온다(명세: UE FString)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FString AccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FString LoginId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FString Nickname;

	//가입 시각(UTC)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FDateTime CreatedAt;
};

//~ Begin 로그인 API DTO (POST /auth/login, 첫 흐름 API 명세 LoginRequest·LoginSuccess)
//요청·응답 JSON과 같은 모양으로 둔다. 칸 이름이 바뀌면 명세부터 고치고 여기를 맞춘다.

/**
 * 로그인 요청 본문. 서버는 모르는 칸이 오면 400 INVALID_REQUEST_BODY로 거절하므로 명세 칸만 둔다.
 * 비밀번호가 들어 있으므로 이 구조체나 변환한 JSON을 로그에 남기지 않는다.
 */
USTRUCT()
struct PROJECTWARRIOR_API FWarriorLoginRequestDto
{
	GENERATED_BODY()

	//빈 값만 거절된다(형식 규칙 없음)
	UPROPERTY()
	FString LoginId;

	UPROPERTY()
	FString Password;
};

/**
 * 메인화면 값(계정 스냅샷, 명세 AccountSnapshot). 로그인 응답의 account, GET /accounts/me의 data.
 * FWarriorAccountData와 같은 뜻이지만 JSON 모양이 달라(태그가 문자열) 이 구조로 받은 뒤 옮긴다.
 * 칸 이름이 JSON과 다르면 오류 없이 기본값(레벨 1 등)으로 남으므로, 명세(account-api.md) 칸 이름과 정확히 맞춘다.
 */
USTRUCT()
struct PROJECTWARRIOR_API FWarriorAccountSnapshotDto
{
	GENERATED_BODY()

	//계정 ID. DB에서는 BIGINT지만 JSON에서는 문자열로 온다
	UPROPERTY()
	FString AccountId;

	//계정이 바뀔 때마다 오르는 번호. 지금 적용된 값보다 작으면 무시한다
	UPROPERTY()
	int64 Version = 0;

	//계정 레벨. 1부터 (FWarriorAccountData::AccountLevel). JSON 이름 "accountLevel"
	UPROPERTY()
	int32 AccountLevel = 1;

	//현재 레벨 안에서 모은 경험치 (FWarriorAccountData::Experience)
	UPROPERTY()
	int32 Experience = 0;

	//아직 쓰지 않은 스탯 포인트 (FWarriorAccountData::StatPoints)
	UPROPERTY()
	int32 StatPoints = 0;

	//스탯 태그 문자열 → 투자한 포인트. 예: {"Account.Stat.MaxHealth": 2}
	UPROPERTY()
	TMap<FString, int32> InvestedStats;

	//해금한 스킬 태그 문자열. 예: ["Account.Skill.DodgeMastery"]
	UPROPERTY()
	TArray<FString> UnlockedSkills;

	//누적 경험치는 서버 계산용이라 보내지 않는다(account-api.md). FWarriorAccountData::TotalExperience는 게임 로컬 값이다
};

//로그인 응답의 data (명세 LoginResult)
USTRUCT()
struct PROJECTWARRIOR_API FWarriorLoginResultDto
{
	GENERATED_BODY()

	//인증 헤더에 붙일 43자 토큰(불투명 토큰, JWT 아님). 메모리에만 두고 로그에 남기지 않는다
	UPROPERTY()
	FString AccessToken;

	//항상 "Bearer"
	UPROPERTY()
	FString TokenType;

	//세션 만료 시각(UTC ISO-8601). 세션은 인증 요청마다 10분씩 늘어나므로 곧 지난 값이 된다(참고용). JSON 이름 "sessionExpiresAt"
	UPROPERTY()
	FString SessionExpiresAt;

	//메인화면 값(account-api.md 계정 스냅샷). 로그인 응답에는 loginId·nickname이 없다
	UPROPERTY()
	FWarriorAccountSnapshotDto Account;
};

//로그인 성공(200) 응답 본문 전체: { "data": LoginResult, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorLoginResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorLoginResultDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};
//~ End 로그인 API DTO

//~ Begin 접속 점검 API DTO (POST /auth/heartbeat, auth-api.md A4)

//접속 점검 응답의 data
USTRUCT()
struct PROJECTWARRIOR_API FWarriorHeartbeatResultDto
{
	GENERATED_BODY()

	//늘어난 세션 만료 시각(UTC ISO-8601). 참고용이다(판단은 서버의 401로 한다)
	UPROPERTY()
	FString SessionExpiresAt;
};

//접속 점검 성공(200) 응답 본문 전체: { "data": { sessionExpiresAt }, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorHeartbeatResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorHeartbeatResultDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};
//~ End 접속 점검 API DTO

//~ Begin 회원가입 API DTO (POST /auth/signup, 첫 흐름 API 명세 SignupRequest·SignupSuccess)

/**
 * 회원가입 요청 본문. 명세 칸만 둔다(모르는 칸은 400 INVALID_REQUEST_BODY).
 * 비밀번호 확인 칸은 화면에서만 비교하고 보내지 않는다. 비밀번호가 들어 있으므로 로그에 남기지 않는다.
 */
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSignupRequestDto
{
	GENERATED_BODY()

	//영문 대소문자·숫자 4~20자. 대소문자를 구분하고 입력 그대로 저장된다. 유일
	UPROPERTY()
	FString LoginId;

	UPROPERTY()
	FString Password;

	//화면에 보이는 이름. 유일
	UPROPERTY()
	FString Nickname;

	//선택 입력. 값을 넣지 않으면 JSON에서 칸이 빠진다(빈 문자열 ""을 보내면 서버가 형식 오류로 볼 수 있다)
	UPROPERTY()
	TOptional<FString> Email;
};

//가입한 계정 (명세 CreatedAccount)
USTRUCT()
struct PROJECTWARRIOR_API FWarriorCreatedAccountDto
{
	GENERATED_BODY()

	//계정 ID. JSON에서는 문자열로 온다
	UPROPERTY()
	FString AccountId;

	//서버가 저장한 아이디(입력 그대로). 로그인 화면에는 이 값을 채운다
	UPROPERTY()
	FString LoginId;

	UPROPERTY()
	FString Nickname;

	//만든 시각. UTC ISO-8601 문자열
	UPROPERTY()
	FString CreatedAt;
};

//회원가입 성공(201) 응답 본문 전체: { "data": CreatedAccount, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSignupResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorCreatedAccountDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};
//~ End 회원가입 API DTO
