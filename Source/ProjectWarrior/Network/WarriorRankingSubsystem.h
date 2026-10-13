// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorRankingTypes.h"
#include "WarriorRankingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorRankingBoardUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorRankingFailed, EWarriorRankingFailure, Failure, const FString&, ErrorCode);

/**
 * 랭킹을 서버에서 받아 들고 있다. 명세: 루트 docs/contracts/ranking-api.md v1
 *
 * - RequestBoard: 보드를 바꾸거나 새로 고칠 때. 목록 첫 묶음(RK1·RK3·RK5·RK7)과 내 줄(RK2·RK4·RK6·RK8)을 함께 받고, 둘 다 오면 OnBoardUpdated.
 *   InLimit은 한 묶음 줄 수(스테이지 순위 카드 5, 메인 랭킹 20).
 * - RequestMore: "더 보기". 서버가 준 커서를 그대로 보내고 뒤에 이어 붙인 뒤 OnBoardUpdated. 커서 오류(RANK_INVALID_CURSOR)면 알리지 않고 첫 묶음부터 다시.
 * - 실패 판정은 ClassifyRankingFailure 한 곳. 화면은 실패 종류로 문구·다시 불러오기를 정한다.
 * - 로그인 상태가 끝나면(OnSessionEnded) 받은 값을 모두 비운다(다른 계정이 이어받지 않게).
 * - 늦게 온 지난 요청의 응답은 버린다(요청 번호). 화면은 한 번에 하나의 보드만 본다(메인 랭킹 팝업, 스테이지 순위 카드).
 *
 * 연동 지점(다른 모듈):
 * - 요청 길: UWarriorAuthSubsystem::SendAuthorized(토큰·401 처리). S5가 요청 길을 바꾸면 Send 한 곳만 바꾼다.
 * - 오류 분류·문구: S5의 오류 코드 처리가 생기면 ClassifyRankingFailure를 그쪽 분류로 바꾼다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorRankingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorRankingSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//보드의 첫 묶음과 내 줄을 받는다. 스테이지·지역 보드는 TargetId, 전체 총합·진행 보드는 TargetId 없이. 진행 보드는 기간 All만
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ranking")
	void RequestBoard(EWarriorRankingBoardKind InKind, FName InTargetId, EWarriorRankingPeriod InPeriod, int32 InLimit = 20);

	//지금 보드의 다음 묶음. 더 없거나 받는 중이면 아무것도 하지 않는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ranking")
	void RequestMore();

	UFUNCTION(BlueprintPure, Category = "Warrior|Ranking")
	const FWarriorRankingBoardKey& GetBoardKey() const { return BoardKey; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Ranking")
	const TArray<FWarriorRankingEntry>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Ranking")
	const FWarriorRankingMine& GetMine() const { return Mine; }

	//응답 board.periodKey("all" 또는 "2026-W42")
	UFUNCTION(BlueprintPure, Category = "Warrior|Ranking")
	const FString& GetPeriodKey() const { return PeriodKey; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Ranking")
	bool HasMore() const { return !NextCursor.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Warrior|Ranking")
	bool IsRequestingBoard() const { return bRequestingBoard; }

	/**
	 * 실패 응답을 화면이 쓰는 종류로 나눈다. OutCode는 서버 code(또는 NETWORK_ERROR·HTTP_<상태>·INVALID_RESPONSE).
	 * Status 0 = 연결 실패, -1 = 응답을 읽지 못함.
	 * S5 연동 지점: S5의 오류 코드 처리(코드 → 종류·문구)가 생기면 이 함수 안쪽만 그쪽 분류로 바꾼다(부르는 쪽은 그대로).
	 */
	static EWarriorRankingFailure ClassifyRankingFailure(int32 InStatus, const FString& InBody, FString& OutCode);

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Ranking")
	FOnWarriorRankingBoardUpdated OnBoardUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Ranking")
	FOnWarriorRankingFailed OnBoardFailed;

private:
	//GET 하나. 보내지 못하면(로그인 안 됨) false
	bool Send(const FString& InPath, TFunction<void(int32, const FString&)>&& OnDone);

	FString BuildListPath(const FWarriorRankingBoardKey& InKey, const FString& InCursor) const;
	FString BuildMinePath(const FWarriorRankingBoardKey& InKey) const;

	void HandleListResponse(int32 InSerial, bool bInAppend, int32 Status, const FString& Body);
	void HandleMineResponse(int32 InSerial, int32 Status, const FString& Body);

	//첫 묶음과 내 줄이 둘 다 왔으면 내 줄 표시를 붙이고 알린다
	void FinishBoardIfReady(int32 InSerial);
	void FailBoard(int32 InSerial, int32 Status, const FString& Body);
	void MarkMine();

	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage);

	FWarriorRankingBoardKey BoardKey;
	int32 Limit = 20;
	FString PeriodKey;

	UPROPERTY(Transient)
	TArray<FWarriorRankingEntry> Entries;

	UPROPERTY(Transient)
	FWarriorRankingMine Mine;

	FString NextCursor;
	bool bRequestingBoard = false;
	bool bListArrived = false;
	bool bMineArrived = false;

	//RequestBoard·세션 끝마다 오른다. "더 보기" 응답도 이 번호로 가린다
	int32 BoardSerial = 0;
};
