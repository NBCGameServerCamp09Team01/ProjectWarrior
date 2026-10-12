// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorApiTypes.generated.h"

/**
 * 웹서버 API 공통 응답 형식(첫 흐름 API 명세 "공통 규칙", 루트 docs/contracts/).
 * - 성공: { "data": …, "meta": { "requestId": … } }  → 기능별 응답 DTO가 Data·Meta를 가진다
 * - 실패: { "code", "message", "path", "retryable", "errors" } → FWarriorApiError
 *
 * FJsonObjectConverter는 C++ 이름의 첫 글자만 소문자로 바꿔 JSON 이름과 맞춘다(RequestId ↔ requestId).
 * 그래서 bool도 b 접두어 없이 쓴다(bRetryable로 쓰면 "bRetryable"이 되어 "retryable"과 맞지 않는다).
 */

//성공 응답의 meta
USTRUCT()
struct PROJECTWARRIOR_API FWarriorApiMeta
{
	GENERATED_BODY()

	//요청 번호. 응답 헤더 X-Request-Id와 같고 서버 로그에 남는다. 문의할 때 이 값을 알려 주면 서버 로그를 찾을 수 있다
	UPROPERTY()
	FString RequestId;
};

//VALIDATION_FAILED일 때 칸별 오류 하나
USTRUCT()
struct PROJECTWARRIOR_API FWarriorApiFieldError
{
	GENERATED_BODY()

	//요청 칸 이름 (예: "loginId")
	UPROPERTY()
	FString Field;

	UPROPERTY()
	FString Message;
};

//실패 응답 본문 (모든 API 공통). 연결 실패처럼 응답이 없을 때는 게임이 Code를 직접 채운다(예: NETWORK_ERROR).
//해석은 UWarriorApiSubsystem::ParseError 한 곳에서 한다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorApiError
{
	GENERATED_BODY()

	//프로그램이 구분하는 오류 코드. 화면 문구는 이 값으로 고른다 (예: AUTH_INVALID_CREDENTIALS)
	UPROPERTY()
	FString Code;

	//사람이 읽는 설명(개발 확인용). 화면에 그대로 쓰지 않는다
	UPROPERTY()
	FString Message;

	//요청 경로
	UPROPERTY()
	FString Path;

	//같은 요청을 다시 보내도 되는지 (503·500은 true). 칸 이름은 README 공통 규칙 PR에서 확정 예정
	UPROPERTY()
	bool Retryable = false;

	//VALIDATION_FAILED일 때만 온다
	UPROPERTY()
	TArray<FWarriorApiFieldError> Errors;

	//429 AUTH_LOGIN_LOCKED일 때만 온다. 남은 잠김 초(Retry-After 헤더와 같은 값). 없으면 0
	UPROPERTY()
	int32 RetryAfterSeconds = 0;

	//HTTP 상태(0은 응답 없음). JSON 칸이 아니라 게임이 해석할 때 채운다(UPROPERTY가 아니므로 JSON 변환에서 빠진다)
	int32 HttpStatus = 0;
};

//~ Begin S5 오류 처리 (UWarriorApiSubsystem·FWarriorErrorText·UWarriorPendingRequestStore)

//오류를 보여 주는 방법. BP에 저장될 수 있으므로 새 값은 끝에 더한다
UENUM(BlueprintType)
enum class EWarriorErrorPresentation : uint8
{
	LogOnly,		// 화면에 보이지 않음
	Inline,			// 화면 안 문구(로그인·가입 화면 아래 문구)
	Field,			// 입력 칸 아래 문구
	Popup,			// 공용 팝업
	ScreenState		// 화면별 실패 상태(스테이지 선택 실패 영역 등)
};

//오류를 보인 뒤 할 일. BP에 저장될 수 있으므로 새 값은 끝에 더한다
UENUM(BlueprintType)
enum class EWarriorErrorAction : uint8
{
	None,			// 그대로 둠
	Retry,			// 다시 보냄(자동 재시도 → "다시 시도 / 종료")
	GoLogin,		// 로그인 화면
	GoTitle,		// 타이틀
	GoMainMenu,		// 메인메뉴
	Quit,			// 게임 종료
	DiscardStored	// 보관본을 지움(서버가 다시 받지 않는 요청)
};

//오류 코드 하나에 대한 규칙(문구 표 한 줄). 표는 Network/WarriorErrorText.cpp
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorErrorRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	FString Code;

	//플레이어에게 보이는 문구
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	EWarriorErrorPresentation Presentation = EWarriorErrorPresentation::Popup;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	EWarriorErrorAction Action = EWarriorErrorAction::None;

	//게임 쪽 버그로 생기는 오류(요청 모양이 명세와 다름 등). Error 로그를 남긴다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	bool bClientBug = false;
};

//보관할 수 있는 요청 종류. 보관 파일에 이름(문자열)으로 저장되므로 이름을 바꾸지 않고 새 값은 끝에 더한다
UENUM(BlueprintType)
enum class EWarriorPendingKind : uint8
{
	None,			// 보관하지 않음
	StageResult		// 스테이지 결과 제출(result-api.md R1)
	//예약: StatInvest(S3), SkillUnlock(S4), MailClaim(우편 받기), PlayCheckpoint(M22 로컬 사본)
};

//UWarriorApiSubsystem::Send에 넘기는 요청 하나
USTRUCT()
struct PROJECTWARRIOR_API FWarriorApiRequest
{
	GENERATED_BODY()

	//GET·POST
	FString Verb = TEXT("GET");

	//BaseUrl 뒤에 붙는 경로. 예: /accounts/me/stage-plays
	FString Path;

	//JSON 본문. 비어 있으면 본문 없이 보낸다
	FString Body;

	//Authorization 헤더를 붙일지
	bool bAuth = true;

	//계속 실패하면 보관할 종류. None이면 보관하지 않는다
	EWarriorPendingKind StoreKind = EWarriorPendingKind::None;

	//본문의 requestId(재전송 키). 보관 파일 이름이 된다
	FString RequestId;

	//보관 종류 안에서 찾을 때 쓰는 키. 결과 제출이면 stagePlayId
	FString StoreKey;

	//보관본이 유효한 마지막 시각(UTC). 비어 있으면 설정의 기본 유효 기간
	FDateTime StoreExpiresAt;

	//자동 재시도가 모두 실패하면 "다시 시도 / 종료"를 물을지. 조회·재전송은 묻지 않는다
	bool bAskOnFail = false;

	//문구를 고를 때 쓰는 화면 맥락(예: Login). 없으면 코드의 기본 문구
	FName Context;
};

//UWarriorApiSubsystem::Send의 결과 하나
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorApiResult
{
	GENERATED_BODY()

	//2xx
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	bool bOk = false;

	//0은 응답 없음
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	int32 HttpStatus = 0;

	//응답 본문(성공이면 { data, meta })
	FString Body;

	//실패면 해석한 오류
	FWarriorApiError Error;

	//실패면 문구 표에서 찾은 규칙
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	FWarriorErrorRule Rule;

	//실패해서 디스크에 보관했는가
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	bool bStored = false;

	//보낸 횟수(처음 1)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	int32 Attempts = 0;
};

//팝업 담당(UWarriorErrorPresenterSubsystem)에 띄워 달라고 넘기는 안내 하나
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorErrorNotice
{
	GENERATED_BODY()

	//오류 코드(없으면 빈 값). 로그용
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	FString Code;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	FText Message;

	//팝업을 닫은 뒤 할 일. Quit이면 확인을 누르면 게임을 끝낸다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	EWarriorErrorAction Action = EWarriorErrorAction::None;

	//"다시 시도 / 종료"를 묻는 팝업인가. 답은 UWarriorApiSubsystem::AnswerRetryPrompt로 돌려준다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Api")
	bool bAskRetry = false;
};

//~ End S5 오류 처리
