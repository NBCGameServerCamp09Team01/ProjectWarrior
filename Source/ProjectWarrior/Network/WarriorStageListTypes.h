// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStageListTypes.generated.h"

/**
 * 스테이지 선택 팝업이 그리는 목록(UWarriorStageListSubsystem이 만든다).
 * 서버 ST1(GET /stages, 정의)과 ST2(GET /accounts/me/stages, 내 진행)를 stageId로 합친 값이다.
 * 명세: 루트 docs/contracts/stage-api.md v1.1
 */

//서버 ST2의 status. 열림·잠김 규칙은 서버가 계산하고 게임은 이 값대로 그리기만 한다.
//BP에 저장된 값이 바뀌지 않도록 새 값(기간 이벤트·업데이트 필요 등 확장 상태)은 끝에 추가한다.
UENUM(BlueprintType)
enum class EWarriorStageEntryStatus : uint8
{
	Locked,
	Open,
	Cleared
};

/** 웨이브 한 줄의 몬스터 한 종류. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageWaveEnemyInfo
{
	GENERATED_BODY()

	//몬스터 키(서버 monsterId, 예: monster.samurai). M24부터 서버 스테이지 목록(ST1) 값
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FString EnemyKey;

	//화면 이름(ST1 monsters[].name). 서버 목록에 이름이 없으면 몬스터 키
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bBoss = false;
};

/** 웨이브 하나의 구성. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageWaveInfo
{
	GENERATED_BODY()

	//1부터
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bBossWave = false;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	TArray<FWarriorStageWaveEnemyInfo> Enemies;
};

/** 스테이지 한 줄. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageListEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName StageId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName RegionId;

	//서버 name
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FText DisplayName;

	//지역 안 순서. 1부터
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 Order = 0;

	//이 스테이지를 열려면 클리어해야 하는 스테이지. 첫 스테이지는 None
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName RequiresStageId;

	//서버 waveCount(결과 검사 기준)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 WaveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	EWarriorStageEntryStatus Status = EWarriorStageEntryStatus::Locked;

	//서버가 처음 클리어 결과를 받아들인 시각(UTC). 클리어 전에는 비어 있다(bHasFirstCleared false)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FDateTime FirstClearedAt;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bHasFirstCleared = false;

	//입장 버튼을 누를 수 있는가(열림·클리어)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCanEnter = false;

	//웨이브 구성. 지금은 UE 웨이브 DataAsset에서 읽는다(UWarriorStageListSubsystem::BuildWaveInfo 한 곳)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	TArray<FWarriorStageWaveInfo> Waves;
};

/** 지역 한 칸과 그 안의 스테이지. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorRegionListEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName RegionId;

	//서버 name
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 Order = 0;

	//지역을 열 수 있는가. 지금 규칙은 늘 열림(확장 여지: UWarriorStageListSubsystem::IsRegionOpen 한 곳에서 바꾼다)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCanOpen = true;

	//이 지역에서 클리어한 스테이지 수(진행 점 표시)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 ClearedCount = 0;

	//order 순
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	TArray<FWarriorStageListEntry> Stages;
};
