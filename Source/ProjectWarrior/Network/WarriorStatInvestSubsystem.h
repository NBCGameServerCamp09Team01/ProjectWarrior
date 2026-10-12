// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorStatInvestSubsystem.generated.h"

//투자 요청이 끝났다. 성공이면 계정 값은 이미 적용돼 OnAccountChanged가 지나갔다. 실패면 ErrorCode(예: STAT_POINTS_NOT_ENOUGH)와 화면 문구
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWarriorStatInvestCompleted, bool, bSuccess, const FString&, ErrorCode, const FText&, Message);

/**
 * 스탯 투자 서브시스템. 성장 화면의 "적용"을 받아 묶음 투자를 서버에 보내고, 응답의 계정 값을 적용한다.
 * 명세: 루트 docs/contracts/stat-api.md v1(T1 POST /accounts/me/stat-investments)
 *
 * - 로그인 상태: 서버에 보낸다(UWarriorAuthSubsystem::SendAuthorized). 서버가 포인트·최대치·0 아래를 판정하고 전부 반영하거나 전부 거절한다.
 *   성공하면 응답 account를 UWarriorAuthSubsystem::ApplyAccountSnapshot으로 적용한다(로그인 응답과 같은 길).
 * - retryable(연결 실패·REQUEST_IN_PROGRESS·VERSION_CONFLICT·5xx)이면 같은 본문(같은 requestId)으로 다시 보낸다(최대 InvestMaxAttempts번).
 *   서버는 같은 requestId를 한 번만 반영한다.
 * - 로그인하지 않았으면 보내지 않고 NOT_LOGGED_IN으로 끝낸다. 로컬 저장·반영 길은 없다(서버가 최종 기준. 시험할 때도 서버를 켠다).
 * - 한 번에 요청 하나. 응답을 기다리는 동안 새 요청은 받지 않는다(화면은 IsInvestInFlight로 버튼을 막는다).
 * - b의 S5가 통신 서브시스템(UF-1)을 만들면 SendAuthorized 호출 한 줄을 옮긴다.
 */
UCLASS(Config = Game)
class PROJECTWARRIOR_API UWarriorStatInvestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStatInvestSubsystem* Get(const UObject* WorldContextObject);

	//묶음 투자. InChanges는 스탯 태그 → 바꿀 포인트(양수 올림, 음수 내림, 0은 무시).
	//요청을 시작했으면 true(결과는 OnInvestCompleted). 보낼 것이 없거나, 이미 요청 중이거나, 시작하지 못하면 false.
	//로그인하지 않았으면 OnInvestCompleted(false, NOT_LOGGED_IN)를 바로 보내고 false
	bool RequestInvest(const TMap<FGameplayTag, int32>& InChanges);

	UFUNCTION(BlueprintPure, Category = "Warrior|Growth")
	bool IsInvestInFlight() const { return bInFlight; }

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Growth")
	FOnWarriorStatInvestCompleted OnInvestCompleted;

protected:
	//투자 요청을 보내는 최대 횟수(처음 1번 + retryable일 때 다시). 1 이상
	UPROPERTY(Config)
	int32 InvestMaxAttempts = 2;

private:
	bool SendInvest(const FString& InRequestBody, int32 InAttempt);

	void HandleInvestResponse(const FString& InRequestBody, int32 InAttempt, int32 Status, const FString& Body);

	void Finish(bool bInSuccess, const FString& InCode, const FText& InMessage);

	//오류 코드로 성장 화면 문구를 고른다(b의 문구 표 UF-4가 생기기 전까지 여기)
	static FText ErrorToText(const FString& InCode, int32 InStatus);

	bool bInFlight = false;
};
