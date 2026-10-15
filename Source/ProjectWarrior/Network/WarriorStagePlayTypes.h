// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "ProjectWarrior/Stage/WarriorPlayCheckpointTypes.h"
#include "ProjectWarrior/Stage/WarriorWavePlan.h"
#include "WarriorStagePlayTypes.generated.h"

/**
 * 스테이지 플레이 API DTO(루트 docs/contracts/stage-play-api.md v1).
 * 칸 이름은 FJsonObjectConverter가 첫 글자만 소문자로 바꿔 맞춘다(StagePlayId ↔ stagePlayId).
 * 요청 DTO에 서버가 모르는 칸이 있으면 400이므로 명세에 있는 칸만 둔다.
 */

//~ Begin 시작 (POST /accounts/me/stage-plays)

//시작 요청 본문
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

	//선택 화면에서 고른 난이도(M9, stage-play-api.md v1.4). 서버 difficulties.json에 없는 번호면 400
	UPROPERTY()
	int32 Difficulty = 0;
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

	//결과를 낼 수 있는 마지막 시각(UTC ISO-8601, 시작 + 24시간). 결과를 보관할 때 보관 유효 기간으로 쓴다(S5)
	UPROPERTY()
	FString ExpiresAt;

	//endReason·endedAt은 진행 중이면 null로 온다. 게임이 쓰지 않으므로 칸을 두지 않는다(null을 FString으로 읽다 실패하지 않게)

	//시작 때 고정한 웨이브 구성(M24, 응답의 waveVersion·waves). JSON 칸 이름과 달라 FJsonObjectConverter는 건드리지 않고,
	//응답을 받은 곳에서 WarriorWavePlan::ReadFromResponse로 채운다(waves·spawnGroup이 null일 수 있음)
	UPROPERTY()
	FWarriorStageWavePlan WavePlan;
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

//~ Begin 내 진행 중 플레이 (GET /accounts/me/stage-plays/current)

//진행 중 플레이를 물은 결과
UENUM()
enum class EWarriorCurrentStagePlay : uint8
{
	None,		// 204: 진행 중 플레이 없음(마감이 지난 것은 서버가 EXPIRED로 바꾸고 204)
	InProgress,	// 200: 진행 중 플레이 있음
	Failed		// 연결 실패·응답을 읽지 못함·그 밖의 상태
};

//~ End 내 진행 중 플레이

//~ Begin 체크포인트 받기 (GET /accounts/me/stage-plays/{stagePlayId}/checkpoint, C2)

//체크포인트를 물은 결과
UENUM()
enum class EWarriorCheckpointFetch : uint8
{
	None,	// 204: 없음(올린 적 없음, 판이 끝남, 마감 지남)
	Found,	// 200: 있음
	Failed	// 연결 실패·응답을 읽지 못함·그 밖의 상태
};

//C2 응답 본문 전체: { "data": Checkpoint, "meta": Meta }. data의 stagePlayId·savedAt은 게임이 쓰지 않아 읽지 않는다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorPlayCheckpointResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorPlayCheckpoint Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};

//~ End 체크포인트 받기

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

//결과 제출 응답의 data. account(지금 계정 값)는 ApplyAccountSnapshot으로 적용한다(M25, account-api.md 동기화 시점 T2·T5b)
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageResultSubmitDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorStageResultDto Result;

	//SAVED(이번에 저장) · ALREADY_SAVED(이미 저장돼 있어 처음 결과를 돌려줌). 둘 다 저장된 것으로 본다
	UPROPERTY()
	FString SaveStatus;

	//지금 계정 값(ALREADY_SAVED여도 지금 값). 응답에 칸이 없으면 기본값(빈 AccountId)이라 적용하지 않는다
	UPROPERTY()
	FWarriorAccountSnapshotDto Account;
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

//이번 판 결과 제출의 상태. 결과 화면은 Submitting이 끝나야 보인다(서버에 연결되지 않으면 보관 팝업이 먼저 뜬다)
UENUM()
enum class EWarriorResultSubmitState : uint8
{
	None,			// 아직 내지 않음(판 시작 때 되돌린다)
	Submitting,		// 보냈고 답(자동 재시도·"다시 시도 / 보관 후 종료" 포함)을 기다리는 중
	Saved,			// 200: 서버에 저장됨(SAVED·ALREADY_SAVED)
	Rejected,		// 서버가 받지 않음(RESULT_INVALID·STAGE_PLAY_EXPIRED 등). 서버와는 연결됨
	NotSent,		// 보내지 않음(플레이 ID 없음: 로그인 안 한 PIE, 시작 실패). 기다릴 답이 없다
	Offline,		// 서버에 연결되지 않아 저장하지 못함(보관했으면 다음 로그인 때 재전송). 종료 안내로 이어진다
	SessionEnded	// 401: 로그인 상태가 끝남. 메인메뉴로 돌아간다
};

//~ End 결과 제출
