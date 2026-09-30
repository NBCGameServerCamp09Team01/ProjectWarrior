// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WarriorAccountTypes.generated.h"

/**
 * 계정 데이터. 판과 판 사이에 유지되는 값이며 GameInstance 서브시스템이 보관한다.
 *
 * 1주차에는 메모리에만 있다(에디터를 다시 실행하면 초기화).
 * 2주차부터는 서버가 최종 기준이다. 서버가 내려 준 값은 UWarriorAccountSubsystem::ApplyServerSnapshot으로 통째로 덮어쓴다.
 * 그러므로 이 구조는 서버 응답 본문과 같은 모양으로 유지한다(필드를 추가할 때 SchemaVersion을 올린다).
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAccountData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 AccountLevel = 1;

	//현재 레벨 안에서 모은 경험치(다음 레벨까지 채우면 0으로 돌아가며 레벨이 오른다)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 Experience = 0;

	//아직 투자하지 않은 스탯 포인트
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 StatPoints = 0;

	//스탯별 투자한 포인트. 키는 Account.Stat.* 태그
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	TMap<FGameplayTag, int32> InvestedStats;

	//해금한 스킬. Account.Skill.* 태그
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FGameplayTagContainer UnlockedSkills;

	//외부 재화 잔액. 키는 서버가 정하는 재화 이름, 값은 서버가 추적한다.
	//서버 연동 전에는 비어 있고, 서버가 내려 준 값만 들어온다(로컬에서 임의로 늘리지 않는다).
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	TMap<FName, int64> ExternalCurrencies;

	//이미 보상을 준 스테이지 기록 번호. 같은 기록으로 보상이 두 번 들어오지 않게 하는 기준이다.
	//서버 연동 뒤에는 서버가 같은 역할을 하므로, 서버가 내려 준 값이 우선한다.
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	TArray<FGuid> RewardedRecordIds;
};

/** 계정 변경 내역의 종류 */
UENUM(BlueprintType)
enum class EWarriorAccountLedgerType : uint8
{
	StageReward,		// 스테이지 결과로 경험치·포인트를 받음
	StatInvest,			// 스탯 포인트 투자
	SkillUnlock,		// 스킬 해금
	ExternalCurrency	// 외부 재화 변동(서버가 알려 준 값)
};

/**
 * 계정 변경 내역 한 줄.
 *
 * 서버 연동 준비용 기록이다. 서버는 "지금 값"이 아니라 "무엇이 왜 바뀌었는지"를 추적해야 하므로
 * 값이 바뀔 때마다 한 줄씩 남기고, 서버가 확인하기 전까지는 bServerConfirmed를 false로 둔다.
 * EntryId는 서버에 보낼 때 요청 번호(중복 요청 방지 키)로 그대로 쓸 수 있다.
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAccountLedgerEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	EWarriorAccountLedgerType Type = EWarriorAccountLedgerType::StageReward;

	//바뀐 값의 이름. "Exp", "StatPoint" 또는 외부 재화 이름
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FName Currency;

	//증가는 +, 감소는 -
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int64 Delta = 0;

	//투자한 스탯·해금한 스킬의 태그. 해당하지 않으면 비어 있음
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FGameplayTag Target;

	//원인이 된 스테이지 기록 번호(FWarriorStageRecord::RecordId). 해당하지 않으면 비어 있음
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FGuid SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FDateTime TimeUtc;

	//서버가 이 변경을 확인했는가. 서버 연동 전에는 항상 false
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	bool bServerConfirmed = false;
};

/** 스테이지 한 판이 준 보상. 결과 화면이 "레벨 업, 포인트 +n"을 표시할 때 읽는다 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageReward
{
	GENERATED_BODY()

	//보상의 원인이 된 스테이지 기록 번호
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	FGuid RecordId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	bool bCleared = false;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 ExpGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 LevelBefore = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 LevelAfter = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Account")
	int32 StatPointsGained = 0;

	bool DidLevelUp() const { return LevelAfter > LevelBefore; }
};
