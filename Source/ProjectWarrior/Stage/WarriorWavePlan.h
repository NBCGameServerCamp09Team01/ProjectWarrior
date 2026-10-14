// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Stage/WarriorStageWaveTypes.h"
#include "WarriorWavePlan.generated.h"

class FJsonObject;
class UWarriorMonsterCatalogDataAsset;

/**
 * 서버 웨이브 구성(M24): 판 응답(P1~P4) data의 waveVersion·waves(루트 docs/contracts/stage-play-api.md v1.3 "웨이브 구성").
 * 판 시작 때 서버가 고정한 값이고, 그 판은 끝까지 이 값으로 웨이브를 만든다(보통 시작은 P1, 이어하기는 로그인 때 받은 P2).
 * waves·spawnGroup은 null일 수 있어 FJsonObjectConverter로 읽지 않고 WarriorWavePlan::ReadFromResponse로 손으로 읽는다
 * (null을 FString·배열로 읽다 판 응답 전체를 못 읽는 일이 없게).
 */

//웨이브의 적 한 줄(waves[].enemies[]). 몬스터 정의 값을 서버가 풀어 둔 것
USTRUCT()
struct PROJECTWARRIOR_API FWarriorWavePlanEnemy
{
	GENERATED_BODY()

	//몬스터 키(예: monster.samurai). DA_MonsterCatalog로 BP를 찾는다
	UPROPERTY()
	FName MonsterId;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	bool bBoss = false;

	//없으면 None(기본 스폰)
	UPROPERTY()
	FName SpawnGroup;

	UPROPERTY()
	int32 GoldReward = 0;

	UPROPERTY()
	float GoldDropChance = 1.f;
};

//웨이브 하나(waves[])
USTRUCT()
struct PROJECTWARRIOR_API FWarriorWavePlanWave
{
	GENERATED_BODY()

	//1부터
	UPROPERTY()
	int32 WaveNumber = 0;

	//이 웨이브 앞 쉬는 시간(초). 0이면 GameMode 기본
	UPROPERTY()
	float RestSeconds = 0.f;

	UPROPERTY()
	bool bBossWave = false;

	UPROPERTY()
	float GoldChanceMultiplier = 1.f;

	UPROPERTY()
	float GoldAmountMultiplier = 1.f;

	UPROPERTY()
	TArray<FWarriorWavePlanEnemy> Enemies;
};

//판 하나의 웨이브 구성
USTRUCT()
struct PROJECTWARRIOR_API FWarriorStageWavePlan
{
	GENERATED_BODY()

	//웨이브 정보 버전(예: w1-m1). 로그·버그 신고용
	UPROPERTY()
	FString WaveVersion;

	//웨이브 번호 순. 응답에 waves가 없거나 null이면 비어 있다(그 판은 시작하지 않음)
	UPROPERTY()
	TArray<FWarriorWavePlanWave> Waves;

	bool IsSet() const { return Waves.Num() > 0; }
};

namespace WarriorWavePlan
{
	//판 응답 본문({ data, meta })의 data.waveVersion·data.waves를 읽는다. 없거나 null이면 OutPlan은 빈 값(IsSet false)
	PROJECTWARRIOR_API void ReadFromResponse(const FString& InBody, FWarriorStageWavePlan& OutPlan);

	//data 객체에서 읽는다(ReadFromResponse가 부른다. 시험용으로도 씀)
	PROJECTWARRIOR_API void ReadFromData(const TSharedPtr<FJsonObject>& InData, FWarriorStageWavePlan& OutPlan);

	//구성의 몬스터 키가 모두 카탈로그에 있는가. 없는 첫 키를 OutUnknown에 넣는다(카탈로그가 없으면 첫 키)
	PROJECTWARRIOR_API bool FindUnknownMonster(const FWarriorStageWavePlan& InPlan, const UWarriorMonsterCatalogDataAsset* InCatalog, FName& OutUnknown);

	//구성 → FlowManager가 쓰는 웨이브 데이터. 몬스터 BP를 여기서 로드한다(LoadSynchronous).
	//구성이 비었거나 모르는 몬스터가 하나라도 있으면 false(OutUnknown에 그 키, 구성이 비었으면 None)
	PROJECTWARRIOR_API bool ToWaveData(const FWarriorStageWavePlan& InPlan, const UWarriorMonsterCatalogDataAsset* InCatalog,
		TArray<FWarriorStageWaveData>& OutWaves, FName& OutUnknown);
}
