// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorStagePlayTypes.generated.h"

/**
 * 스테이지 플레이 API DTO(루트 docs/contracts/stage-play-api.md v1).
 * 칸 이름은 FJsonObjectConverter가 첫 글자만 소문자로 바꿔 맞춘다(StagePlayId ↔ stagePlayId).
 * 요청 DTO에 서버가 모르는 칸이 있으면 400이므로 명세에 있는 칸만 둔다.
 */

//~ Begin 시작 (POST /accounts/me/stage-plays)

//시작 요청 본문. 난이도는 기본 0이라 보내지 않는다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStagePlayStartRequestDto
{
	GENERATED_BODY()

	//같은 시작을 다시 보낼 때 같은 값. 소문자 하이픈 GUID
	UPROPERTY()
	FString RequestId;

	//스테이지 키(소문자). 예: stage.01.01
	UPROPERTY()
	FString StageId;
};

//스테이지 플레이 하나(시작·현황·조회·포기 응답의 data). 게임이 쓰지 않는 칸(accountId·시각)은 두지 않는다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStagePlayDto
{
	GENERATED_BODY()

	UPROPERTY()
	FString StagePlayId;

	UPROPERTY()
	FString StageId;

	UPROPERTY()
	int32 Difficulty = 0;

	//서버가 결과를 검사할 때 쓰는 웨이브 수
	UPROPERTY()
	int32 WaveCount = 0;

	//IN_PROGRESS·CLEARED·FAILED·EXPIRED
	UPROPERTY()
	FString Status;

	//endReason·endedAt은 진행 중이면 null로 온다. 게임이 쓰지 않으므로 칸을 두지 않는다(null을 FString으로 읽다 실패하지 않게)
};

//스테이지 플레이 응답 본문 전체: { "data": StagePlay, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStagePlayResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorStagePlayDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};

//~ End 시작

//~ Begin 결과 제출 (POST /accounts/me/stage-plays/{stagePlayId}/result, result-api.md)

//결과 요청 본문. FWarriorStageResult를 그대로 변환하면 bCleared가 "bCleared"로 나가 400이므로 칸을 따로 둔다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageResultRequestDto
{
	GENERATED_BODY()

	//재전송 때 같은 값. 소문자 하이픈 GUID
	UPROPERTY()
	FString RequestId;

	UPROPERTY()
	FString StageId;

	UPROPERTY()
	int32 Difficulty = 0;

	UPROPERTY()
	bool Cleared = false;

	UPROPERTY()
	int32 ReachedWave = 0;

	UPROPERTY()
	int32 TotalWaveCount = 0;

	UPROPERTY()
	float PlayTimeSeconds = 0.f;

	UPROPERTY()
	int32 EarnedGold = 0;

	UPROPERTY()
	int32 KillCount = 0;
};

//서버가 계산한 보상(UE FWarriorStageReward와 같은 칸)
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageRewardDto
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ExpGained = 0;

	UPROPERTY()
	int32 LevelBefore = 0;

	UPROPERTY()
	int32 LevelAfter = 0;

	UPROPERTY()
	int32 StatPointsGained = 0;
};

//저장된 결과 하나(제출 응답의 data.result, 결과 다시 받기의 data). 게임이 쓰는 칸만 둔다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageResultDto
{
	GENERATED_BODY()

	UPROPERTY()
	FString StagePlayId;

	UPROPERTY()
	FString StageId;

	UPROPERTY()
	bool Cleared = false;

	UPROPERTY()
	FWarriorStageRewardDto Reward;
};

//결과 제출 응답의 data. account(바뀐 계정 값)는 확장 칸이라 지금은 읽지 않는다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageResultSubmitDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorStageResultDto Result;

	//SAVED(이번에 저장) · ALREADY_SAVED(이미 저장돼 있어 처음 결과를 돌려줌). 둘 다 저장된 것으로 본다
	UPROPERTY()
	FString SaveStatus;
};

//결과 제출 응답 본문 전체: { "data": Submit, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageResultSubmitResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorStageResultSubmitDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};

//~ End 결과 제출
