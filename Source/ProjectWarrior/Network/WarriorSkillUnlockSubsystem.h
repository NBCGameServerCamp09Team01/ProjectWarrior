// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorSkillUnlockTypes.h"
#include "WarriorSkillUnlockSubsystem.generated.h"

class FJsonValue;

//해금 요청이 끝났다. 성공이면 계정 값은 이미 적용돼 OnAccountChanged가 지나갔다.
//실패면 ErrorCode(예: SKILL_REQUIREMENTS_NOT_MET)와 화면 문구. 조건 미충족이면 GetLastRequirements에 조건 전부가 있다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWarriorSkillUnlockCompleted, bool, bSuccess, FGameplayTag, SkillTag, const FString&, ErrorCode, const FText&, Message);

//서버 진행도(K2)를 받았다. 실패하면 bSuccess false이고 GetServerSkills는 지난 값 그대로
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorSkillsReceived, bool, bSuccess);

/**
 * 스킬 해금 서브시스템. 스킬 화면의 해금 버튼을 서버에 보내고(K1), 서버가 판정한 진행도를 받아 둔다(K2).
 * 명세: 루트 docs/contracts/skill-api.md v1
 *
 * - 해금: 로그인 상태면 서버에 보낸다(UWarriorAuthSubsystem::SendAuthorized). 서버가 선행·포인트·누적 기록을 판정한다.
 *   성공하면 응답 account를 ApplyAccountSnapshot으로 적용한다(로그인 응답과 같은 길). 그 뒤 진행도를 다시 받는다.
 * - retryable(연결 실패·REQUEST_IN_PROGRESS·VERSION_CONFLICT·5xx)이면 같은 본문(같은 requestId)으로 다시 보낸다(최대 UnlockMaxAttempts번).
 * - 로그인하지 않았으면 보내지 않고 NOT_LOGGED_IN으로 끝낸다. 로컬 해금 길은 없다(S3와 같은 규칙).
 * - 진행도(K2)는 이 PC의 로컬 통계가 아니라 서버 누적 기록으로 판정한 값이다. 받은 세션(로그인)이 바뀌면 버린다.
 * - 한 번에 해금 요청 하나. 응답을 기다리는 동안 새 해금은 받지 않는다(화면은 IsUnlockInFlight로 버튼을 막는다).
 * - b의 S5가 통신 서브시스템(UF-1)을 만들면 SendAuthorized 호출을 옮긴다.
 */
UCLASS(Config = Game)
class PROJECTWARRIOR_API UWarriorSkillUnlockSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorSkillUnlockSubsystem* Get(const UObject* WorldContextObject);

	//스킬 하나 해금(K1). 요청을 시작했으면 true(결과는 OnUnlockCompleted). 이미 요청 중이거나 시작하지 못하면 false.
	//로그인하지 않았으면 OnUnlockCompleted(false, NOT_LOGGED_IN)를 바로 보내고 false
	bool RequestUnlock(FGameplayTag SkillTag);

	//서버 진행도를 받는다(K2). 결과는 OnSkillsReceived. 로그인하지 않았거나 이미 받는 중이면 false
	bool RequestSkills();

	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	bool IsUnlockInFlight() const { return bUnlockInFlight; }

	//지금 로그인 세션에서 받은 서버 진행도가 있나
	bool HasServerSkills() const;

	//마지막으로 받은 서버 진행도(K2). HasServerSkills가 false면 쓰지 않는다
	const TArray<FWarriorServerSkillView>& GetServerSkills() const { return ServerSkills; }

	//마지막 해금 거절(SKILL_REQUIREMENTS_NOT_MET)의 조건 전부. 그 밖의 결과면 비어 있다
	const TArray<FWarriorServerSkillRequirement>& GetLastRequirements() const { return LastRequirements; }

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Skill")
	FOnWarriorSkillUnlockCompleted OnUnlockCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Skill")
	FOnWarriorSkillsReceived OnSkillsReceived;

	//서버 조건 종류 이름 → UE 조건 종류. PREREQUISITE·STAT_POINTS처럼 UE 조건 목록에 없는 종류면 false
	static bool ToRequirementType(const FString& InServerType, EWarriorSkillRequirementType& OutType);

protected:
	//해금 요청을 보내는 최대 횟수(처음 1번 + retryable일 때 다시). 1 이상
	UPROPERTY(Config)
	int32 UnlockMaxAttempts = 2;

private:
	bool SendUnlock(FGameplayTag InSkillTag, const FString& InRequestBody, int32 InAttempt);

	void HandleUnlockResponse(FGameplayTag InSkillTag, const FString& InRequestBody, int32 InAttempt, int32 Status, const FString& Body);

	void HandleSkillsResponse(int32 InSessionSerial, int32 Status, const FString& Body);

	void FinishUnlock(bool bInSuccess, FGameplayTag InSkillTag, const FString& InCode, const FText& InMessage);

	//requirements 배열을 읽는다(current가 null이면 기록 없음)
	static void ParseRequirements(const TArray<TSharedPtr<FJsonValue>>& InValues, TArray<FWarriorServerSkillRequirement>& OutRequirements);

	static EWarriorSkillState ParseState(const FString& InState);

	//오류 코드로 스킬 화면 문구를 고른다(b의 문구 표 UF-4가 생기기 전까지 여기)
	static FText ErrorToText(const FString& InCode, int32 InStatus);

	bool bUnlockInFlight = false;

	bool bSkillsInFlight = false;

	TArray<FWarriorServerSkillView> ServerSkills;

	//ServerSkills를 받은 로그인 세션 번호(UWarriorAuthSubsystem::GetSessionSerial). INDEX_NONE이면 받은 적 없음
	int32 ServerSkillsSessionSerial = INDEX_NONE;

	TArray<FWarriorServerSkillRequirement> LastRequirements;
};
