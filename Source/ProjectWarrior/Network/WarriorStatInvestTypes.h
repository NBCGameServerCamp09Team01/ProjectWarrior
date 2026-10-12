// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorStatInvestTypes.generated.h"

/**
 * 스탯 투자 API DTO(루트 docs/contracts/stat-api.md T1, POST /accounts/me/stat-investments).
 * 칸 이름은 FJsonObjectConverter가 첫 글자만 소문자로 바꿔 맞춘다(StatTag ↔ statTag).
 * 요청 DTO에 서버가 모르는 칸이 있으면 400이므로 명세에 있는 칸만 둔다.
 */

//묶음의 한 줄. Points 양수 = 올림, 음수 = 내림(0은 보내지 않는다)
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStatInvestEntryDto
{
	GENERATED_BODY()

	//스탯 태그 이름. 예: Account.Stat.MaxHealth
	UPROPERTY()
	FString StatTag;

	UPROPERTY()
	int32 Points = 0;
};

//요청 본문. 재전송 때는 같은 본문(같은 RequestId)을 그대로 보낸다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStatInvestRequestDto
{
	GENERATED_BODY()

	//소문자 하이픈 GUID
	UPROPERTY()
	FString RequestId;

	UPROPERTY()
	TArray<FWarriorStatInvestEntryDto> Investments;
};

//투자 결과
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStatInvestResultDto
{
	GENERATED_BODY()

	//반영 뒤 남은 포인트
	UPROPERTY()
	int32 StatPoints = 0;

	//반영 뒤 전체 투자 값(0인 스탯은 빠짐)
	UPROPERTY()
	TMap<FString, int32> InvestedStats;

	UPROPERTY()
	int64 Version = 0;
};

//응답 data. Account는 계정 스냅샷(로그인 응답과 같은 모양)이라 그대로 ApplyAccountSnapshot에 넘긴다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStatInvestDataDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorStatInvestResultDto Result;

	//SAVED · ALREADY_SAVED. 둘 다 반영된 것으로 본다
	UPROPERTY()
	FString SaveStatus;

	UPROPERTY()
	FWarriorAccountSnapshotDto Account;
};

//응답 본문 전체: { "data": Data, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStatInvestResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorStatInvestDataDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};
