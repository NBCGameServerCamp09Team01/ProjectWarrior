// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "ProjectWarrior/Skill/WarriorSkillTypes.h"
#include "WarriorSkillUnlockTypes.generated.h"

/**
 * 스킬 해금 API DTO(루트 docs/contracts/skill-api.md: K1 POST /accounts/me/skill-unlocks, K2 GET /accounts/me/skills).
 * 칸 이름은 FJsonObjectConverter가 첫 글자만 소문자로 바꿔 맞춘다(SkillTag ↔ skillTag).
 * 조건 진행도(requirements)는 current가 null일 수 있어 FJsonObjectConverter로 읽지 않고 직접 읽는다(WarriorSkillUnlockSubsystem).
 */

//K1 요청 본문. 재전송 때는 같은 본문(같은 RequestId)을 그대로 보낸다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSkillUnlockRequestDto
{
	GENERATED_BODY()

	//소문자 하이픈 GUID
	UPROPERTY()
	FString RequestId;

	//스킬 태그 이름. 예: Account.Skill.Combo4
	UPROPERTY()
	FString SkillTag;
};

//K1 해금 결과
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSkillUnlockResultDto
{
	GENERATED_BODY()

	UPROPERTY()
	FString SkillTag;

	//반영 뒤 해금한 스킬 전체(태그 이름순)
	UPROPERTY()
	TArray<FString> UnlockedSkills;

	UPROPERTY()
	int32 StatPoints = 0;

	UPROPERTY()
	int64 Version = 0;
};

//K1 응답 data. Account는 계정 스냅샷이라 그대로 ApplyAccountSnapshot에 넘긴다
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSkillUnlockDataDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorSkillUnlockResultDto Result;

	//SAVED · ALREADY_SAVED. 둘 다 해금된 것으로 본다
	UPROPERTY()
	FString SaveStatus;

	UPROPERTY()
	FWarriorAccountSnapshotDto Account;
};

//K1 응답 본문 전체: { "data": Data, "meta": Meta }
USTRUCT()
struct PROJECTWARRIOR_API FWarriorSkillUnlockResponseDto
{
	GENERATED_BODY()

	UPROPERTY()
	FWarriorSkillUnlockDataDto Data;

	UPROPERTY()
	FWarriorApiMeta Meta;
};

//서버가 판정한 조건 하나(K1 409 requirements, K2 skills[].requirements의 한 줄)
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorServerSkillRequirement
{
	GENERATED_BODY()

	//PREREQUISITE · STAT_POINTS · ACCOUNT_LEVEL · STAGES_CLEARED · TOTAL_KILLS · PLAY_TIME_SECONDS · MAX_WAVE_REACHED · BEST_CLEAR_TIME
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FString Type;

	//PREREQUISITE면 선행 스킬 태그 이름, 나머지는 빈 값
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FString Key;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	double Target = 0.0;

	//bHasCurrent가 false면 의미 없음(기록 없음)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	double Current = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	bool bHasCurrent = true;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	bool bMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	bool bUpperBound = false;
};

//서버가 판정한 스킬 하나(K2 skills[]의 한 줄)
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorServerSkillView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FGameplayTag SkillTag;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	EWarriorSkillState State = EWarriorSkillState::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	int32 Cost = 0;

	//선행 → 비용 → 나머지 조건 순서. 해금한 스킬도 온다(표시용)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	TArray<FWarriorServerSkillRequirement> Requirements;
};
