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

//실패 응답 본문 (모든 API 공통). 연결 실패처럼 응답이 없을 때는 게임이 Code를 직접 채운다(예: NETWORK_ERROR)
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
};
