#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStatTypes.h"
#include "WarriorStageStatsSubsystem.generated.h"

class AWarriorStageGameState;
class UPlayerInventoryComponent;

/**
 * 이번 스테이지 통계 기록기 (S1).
 * 게임 월드마다 생기지만, GameMode가 AWarriorStageGameMode인 월드에서만 기록한다.
 *
 * - 시작: OnWorldBeginPlay에서 GameState를 구독하고 스테이지 기록을 연다.
 * - 진행: GameState의 상태·웨이브 변화로 플레이 시간, 휴식 시간, 웨이브 기록을 채운다.
 *         나머지 값(처치, 데미지 등)은 UWarriorStatsLibrary가 GetMutable* 함수로 채운다 (S1-4).
 * - 종료: StageCleared / StageFailed에서 기록을 마감해 UWarriorProfileStatsSubsystem에 넘긴다.
 *         그 전에 레벨을 떠나면 Deinitialize에서 Abandoned로 마감한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageStatsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStageStatsSubsystem* Get(const UObject* WorldContextObject);

	//~ UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem

	//~ 조회
	/** 스테이지 기록 중이면 true. 스테이지가 아닌 레벨, 클라이언트, 종료 후에는 false */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	bool IsRecording() const { return bRecording && !bRecordSubmitted; }

	/** 진행 중인 스테이지 기록 (이후 HUD용) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const FWarriorStageRecord& GetCurrentStageRecord() const { return Record; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const FWarriorStatBlock& GetCurrentStageStats() const { return Record.Stats; }

	/** 스테이지 시작 기준 경과 시간 (일시정지 중에는 멈춤) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	float GetStageTimeSeconds() const;

	/** 진행 중인 웨이브 번호. 웨이브 전이면 0 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	int32 GetCurrentWaveNumber() const;

	//~ 기록 (UWarriorStatsLibrary 전용). 기록 중이 아니면 nullptr
	FWarriorStageRecord* GetMutableRecord();
	FWarriorStatBlock* GetMutableStats();
	/** 진행 중인 웨이브. 웨이브 전이거나 이미 클리어된 웨이브면 nullptr */
	FWarriorWaveRecord* GetMutableCurrentWave();
	/** 이번 웨이브에 플레이어가 맞았음을 표시 (무피격 웨이브 판정용) */
	void MarkPlayerHitThisWave() { bPlayerHitThisWave = true; }

private:
	void BeginStageRecord(AWarriorStageGameState* InStageGameState);
	void FinishStageRecord(EWarriorStatOutcome InOutcome);

	/** 이전 상태에서 보낸 시간을 플레이 시간·휴식 시간에 더한다 */
	void AccumulateStateTime();
	void OpenWave(int32 InWaveNumber, bool bInBossWave);
	void CloseCurrentWave(bool bInCleared);

	UPlayerInventoryComponent* FindPlayerInventory() const;

	UFUNCTION()
	void HandleStageStateChanged(EWarriorStageState NewState, EWarriorStageState OldState);

	UFUNCTION()
	void HandleWaveChanged(int32 WaveNumber, int32 TotalWaveCount, bool bBossWave);

	static bool IsPlayState(EWarriorStageState InState);

	UPROPERTY(Transient)
	FWarriorStageRecord Record;

	TWeakObjectPtr<AWarriorStageGameState> StageGameState;

	EWarriorStageState CurrentState = EWarriorStageState::None;
	double StageStartWorldTime = 0.0;
	double StateEnterWorldTime = 0.0;
	/** Record.Waves 안에서 진행 중인 웨이브 위치. 없으면 INDEX_NONE */
	int32 CurrentWaveIndex = INDEX_NONE;
	bool bPlayerHitThisWave = false;
	bool bRecording = false;
	bool bRecordSubmitted = false;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FWarriorStatsStageTest;
#endif
};
