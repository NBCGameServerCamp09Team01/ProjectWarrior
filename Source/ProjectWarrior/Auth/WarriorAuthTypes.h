// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorAuthTypes.generated.h"

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
 * FWarriorAccountData와 같은 뜻이지만 JSON 모양이 달라(이름 level, 태그가 문자열) 이 구조로 받은 뒤 옮긴다.
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

	//계정 레벨 (FWarriorAccountData::AccountLevel)
	UPROPERTY()
	int32 Level = 1;

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

	//명세에 totalExperience 칸이 아직 없다. 서버가 칸을 추가하면 int32 TotalExperience를 여기에 더한다
};

//로그인 응답의 data (명세 LoginResult)
USTRUCT()
struct PROJECTWARRIOR_API FWarriorLoginResultDto
{
	GENERATED_BODY()

	//인증 헤더에 붙일 토큰. 메모리에만 두고 로그에 남기지 않는다
	UPROPERTY()
	FString AccessToken;

	//항상 "Bearer"
	UPROPERTY()
	FString TokenType;

	//토큰(또는 세션) 만료 시각. UTC ISO-8601 문자열 → FDateTime::ParseIso8601로 바꾼다
	UPROPERTY()
	FString ExpiresAt;

	//메인화면 값. 로그인 응답에는 loginId·nickname이 없다(닉네임은 명세에서 나중에 칸 추가 예정)
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

//~ Begin 회원가입 API DTO (POST /auth/signup, 첫 흐름 API 명세 SignupRequest·SignupSuccess)

/**
 * 회원가입 요청 본문. 명세 칸만 둔다(모르는 칸은 400 INVALID_REQUEST_BODY).
 * 비밀번호 확인 칸은 화면에서만 비교하고 보내지 않는다. 비밀번호가 들어 있으므로 로그에 남기지 않는다.
 */
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSignupRequestDto
{
	GENERATED_BODY()

	//대소문자 구분 없음, 유일
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

	//서버가 저장한 아이디. 서버가 값을 다듬을 수 있으므로(대소문자 구분 없음) 로그인 화면에는 이 값을 채운다
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
