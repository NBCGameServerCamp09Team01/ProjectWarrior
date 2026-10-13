// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Stats/WarriorStatTypes.h"
#include "WarriorPlayCheckpointTypes.generated.h"

/**
 * 플레이 체크포인트(M22 최종): 진행 중인 판을 웨이브 클리어 시점에서 이어 가기 위한 판 안 값 묶음.
 * 웨이브를 클리어할 때 꺼내(Capture) 서버에 올리고, 이어할 때 받아 넣는다(Restore).
 * 칸 이름은 FJsonObjectConverter가 첫 글자만 소문자로 바꿔 서버 JSON과 맞춘다(ClearedWave ↔ clearedWave).
 * 결정: 계획서(docs_local/m22-play-checkpoint-plan-1013.md) 9장
 */

//인벤토리 한 칸
USTRUCT()
struct PROJECTWARRIOR_API FWarriorCheckpointItem
{
	GENERATED_BODY()

	//아이템 데이터 에셋 경로(UDataAsset_Item). 예: /Game/.../DA_Potion.DA_Potion
	UPROPERTY()
	FString ItemPath;

	UPROPERTY()
	int32 Quantity = 0;
};

//판 강화 하나
USTRUCT()
struct PROJECTWARRIOR_API FWarriorCheckpointUpgrade
{
	GENERATED_BODY()

	//UDataAsset_Upgrade::UpgradeId(저장·서버 전송용 고유 키)
	UPROPERTY()
	FGameplayTag UpgradeId;

	//강화 데이터 에셋 경로. 되돌릴 때 효과(GE)를 다시 걸려면 에셋이 필요하다
	UPROPERTY()
	FString UpgradePath;

	UPROPERTY()
	int32 Level = 0;
};

//GAS 현재값. 최대치·공격력 등은 강화·계정 스탯에서 다시 계산되므로 넣지 않는다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorCheckpointAttributes
{
	GENERATED_BODY()

	UPROPERTY()
	float CurrentHealth = 0.f;

	UPROPERTY()
	float CurrentStamina = 0.f;

	UPROPERTY()
	float CurrentBalance = 0.f;
};

//체크포인트 묶음 하나
USTRUCT()
struct PROJECTWARRIOR_API FWarriorPlayCheckpoint
{
	GENERATED_BODY()

	//묶음 모양의 버전. 칸을 바꾸면 올린다. 이어할 때 다르면 체크포인트를 버리고 처음부터 한다(결정 G1)
	static constexpr int32 CurrentContentVersion = 1;

	UPROPERTY()
	int32 ContentVersion = CurrentContentVersion;

	//방금 클리어한 웨이브 번호(1부터). 이어하면 그다음 웨이브의 쉬는 시간부터 시작한다(결정 C1)
	UPROPERTY()
	int32 ClearedWave = 0;

	//지금까지의 플레이 시간(초). 이어한 판의 결과 시간 = 이 값 + 이어한 뒤 시간(결정 F1)
	UPROPERTY()
	float ElapsedSeconds = 0.f;

	//스테이지 시작 기준 경과 시간(초). 판 통계의 웨이브 시각이 이어지게 맞출 때 쓴다
	UPROPERTY()
	float StageTimeSeconds = 0.f;

	UPROPERTY()
	int32 Gold = 0;

	UPROPERTY()
	TArray<FWarriorCheckpointItem> Items;

	UPROPERTY()
	TArray<FWarriorCheckpointUpgrade> Upgrades;

	UPROPERTY()
	FWarriorCheckpointAttributes Attributes;

	//판 통계(합계·웨이브·구매·사망·적). 이어서 센다. 판 식별값(RecordId·RunId 등)은 되돌릴 때 이번 판 것을 쓴다
	UPROPERTY()
	FWarriorStageRecord StageRecord;

	bool IsSet() const { return ClearedWave > 0; }
};
