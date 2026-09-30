#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorStatTypes.h"
#include "WarriorProfileStatsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorStageRecorded, const FWarriorStageRecord&, StageRecord);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorRunEnded, const FWarriorRunRecord&, RunRecord);

/**
 * GameInstance 통계 보관소 (S1).
 * 게임을 켜 둔 동안 살아 있어 레벨을 옮겨도 유지된다. 디스크 저장은 이후 확장.
 *
 * - 게임 한 판(Run): 스테이지 기록이 처음 들어올 때 시작하고, 실패·마지막 스테이지 클리어·중도 이탈 시 끝난다.
 * - 스테이지 기록은 UWarriorStageStatsSubsystem이 스테이지 종료 시 AddStageRecord로 넘긴다.
 * - 누적(Lifetime)은 스테이지 단위로 합산한다. 판 합계는 판 기록에만 둔다(중복 합산 방지).
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorProfileStatsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 보관 상한 (D4). 넘으면 오래된 것부터 제거하며, 누적 값에는 계속 반영된다 */
	static constexpr int32 MaxStageRecords = 50;
	static constexpr int32 MaxRunRecords = 20;

	static UWarriorProfileStatsSubsystem* Get(const UObject* WorldContextObject);

	/** 프로젝트 설정 ProjectVersion (D6). 비어 있으면 빈 문자열 */
	static FString GetBuildVersion();

	virtual void Deinitialize() override;

	//~ 판(Run)
	/** 진행 중인 판이 없으면 새로 시작한다. 진행 중인 판의 ID를 돌려준다 */
	FGuid BeginRunIfNeeded();

	/** 진행 중인 판을 끝낸다. 진행 중인 판이 없으면 아무것도 하지 않는다. 게임 흐름에서 마지막 스테이지를 판단해 호출 (D9) */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	void EndRun(EWarriorStatOutcome InOutcome);

	/** true면 스테이지 클리어 시 판도 끝낸다. 게임 흐름이 정해지기 전 임시 동작 (D9, 기본 true) */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	void SetEndRunOnStageCleared(bool bInEndRunOnStageCleared) { bEndRunOnStageCleared = bInEndRunOnStageCleared; }

	//~ 스테이지
	/** 끝난 스테이지 기록을 보관하고 판·누적에 반영한다. Outcome이 InProgress면 거부 */
	void AddStageRecord(const FWarriorStageRecord& InStageRecord);

	//~ 조회
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const FWarriorLifetimeStats& GetLifetimeStats() const { return Lifetime; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	bool HasActiveRun() const { return CurrentRun.IsActive(); }

	/** 진행 중인 판. 없으면 RunId가 무효인 빈 값 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const FWarriorRunRecord& GetCurrentRun() const { return CurrentRun; }

	/** 끝난 판 기록 (오래된 순) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const TArray<FWarriorRunRecord>& GetRunRecords() const { return RunRecords; }

	/** 끝난 스테이지 기록 (오래된 순) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const TArray<FWarriorStageRecord>& GetStageRecords() const { return StageRecords; }

	/** 마지막 스테이지 기록. 없으면 false */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	bool GetLastStageRecord(FWarriorStageRecord& OutStageRecord) const;

	/** 마지막 판 기록. 없으면 false */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	bool GetLastRunRecord(FWarriorRunRecord& OutRunRecord) const;

	/** 모든 기록을 지운다 (디버그·테스트용) */
	void ResetAll();

	/** 스테이지 기록이 보관되면 방송 (이후 결과창) */
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stats")
	FOnWarriorStageRecorded OnStageRecorded;

	/** 판이 끝나면 방송 (이후 누적 결과창) */
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stats")
	FOnWarriorRunEnded OnRunEnded;

private:
	void ApplyStageToLifetime(const FWarriorStageRecord& InStageRecord);

	UPROPERTY(Transient)
	FWarriorRunRecord CurrentRun;

	UPROPERTY(Transient)
	TArray<FWarriorRunRecord> RunRecords;

	UPROPERTY(Transient)
	TArray<FWarriorStageRecord> StageRecords;

	UPROPERTY(Transient)
	FWarriorLifetimeStats Lifetime;

	bool bEndRunOnStageCleared = true;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FWarriorStatsProfileTest;
#endif
};
