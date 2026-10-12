// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorRankingTypes.generated.h"

/**
 * 랭킹 화면이 쓰는 값. 명세: 루트 docs/contracts/ranking-api.md v1 (RK1~RK8)
 * 서버 응답의 null 칸(보드에 맞지 않는 칸, 기록 없음)은 bHas…·-1로 나타낸다.
 */

//보드 종류(응답 board.kind STAGE·REGION·TOTAL·PROGRESS). 화면: 스테이지 순위 카드 Stage, 메인 랭킹 Total·Region. Progress는 API만
UENUM(BlueprintType)
enum class EWarriorRankingBoardKind : uint8
{
	Stage,
	Region,
	Total,
	Progress
};

//기간(쿼리 period all·weekly). 진행 보드는 All만
UENUM(BlueprintType)
enum class EWarriorRankingPeriod : uint8
{
	All,
	Weekly
};

//불러오기 실패 종류. 화면은 이 값으로 문구를 고른다(판정: UWarriorRankingSubsystem::ClassifyRankingFailure)
UENUM(BlueprintType)
enum class EWarriorRankingFailure : uint8
{
	None,
	//연결 실패, 500·503, 응답을 읽지 못함 → "현재 서버에서 데이터를 불러올 수가 없습니다." + 다시 불러오기
	ServerUnavailable,
	//RANK_INVALID_CURSOR → 화면에 알리지 않고 첫 묶음부터 다시(서브시스템이 처리)
	InvalidCursor,
	//STAGE_NOT_FOUND·REGION_NOT_FOUND → 게임·서버 키가 어긋난 버그
	NotFound,
	//401 → Auth가 이미 로그인 상태를 끝냈다(로그인 화면으로 가는 흐름)
	SessionEnded,
	//그 밖(400 VALIDATION_FAILED 등, 게임 버그)
	Other
};

//보드 하나(대상 × 기간). 진행은 TargetId가 None
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorRankingBoardKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	EWarriorRankingBoardKind Kind = EWarriorRankingBoardKind::Stage;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	FName TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	EWarriorRankingPeriod Period = EWarriorRankingPeriod::All;

	bool operator==(const FWarriorRankingBoardKey& Other) const
	{
		return Kind == Other.Kind && TargetId == Other.TargetId && Period == Other.Period;
	}
};

//순위 한 줄(응답 items[])
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorRankingEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	FString AccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	FString Nickname;

	//보드 점수(스테이지 점수, 지역 합, 진행은 클리어한 스테이지 수)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	FDateTime AchievedAt;

	//스테이지 보드만. 그 밖은 -1
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	float PlayTimeSeconds = -1.f;

	//지역·전체 총합·진행 보드만. 스테이지 보드는 -1
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	int32 ClearedStageCount = -1;

	//진행 보드만. 그 밖은 None
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	FName FurthestStageId;

	//내 줄인가(내 순위와 같은 순위)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	bool bIsMine = false;
};

//내 줄(RK2·RK4·RK6). 기록이 없으면 bHasRecord false
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorRankingMine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	bool bHasRecord = false;

	//bHasRecord일 때만 의미가 있다(Rank·Score·AchievedAt·상세)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	FWarriorRankingEntry Entry;

	//그 보드에 기록이 있는 계정 수
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Ranking")
	int32 TotalCount = 0;
};

/**
 * 새로 고침 쿨타임(화면마다 하나). 성공 뒤·실패 뒤 다른 시간을 건다(값은 위젯의 EditDefaultsOnly).
 * 시계는 FPlatformTime::Seconds(게임 일시 정지와 상관없음)를 부른 쪽이 넘긴다.
 */
struct PROJECTWARRIOR_API FWarriorRefreshCooldown
{
	void Start(double InNowSeconds, float InCooldownSeconds)
	{
		ReadyAtSeconds = InNowSeconds + FMath::Max(0.f, InCooldownSeconds);
	}

	bool IsReady(double InNowSeconds) const
	{
		return InNowSeconds >= ReadyAtSeconds;
	}

	//남은 초(올림). 준비됐으면 0
	int32 RemainingSeconds(double InNowSeconds) const
	{
		return IsReady(InNowSeconds) ? 0 : FMath::CeilToInt32(ReadyAtSeconds - InNowSeconds);
	}

	void Reset()
	{
		ReadyAtSeconds = 0.0;
	}

private:
	double ReadyAtSeconds = 0.0;
};
