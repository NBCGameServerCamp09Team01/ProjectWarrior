// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameState.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorStageStateChanged, EWarriorStageState, NewState, EWarriorStageState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWarriorWaveChanged, int32, WaveNumber, int32, TotalWaveCount, bool, bBossWave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorEnemyCountChanged, int32, AliveCount, int32, TotalCount);

/**
 * 스테이지 진행 상태를 보관하고 방송한다.
 * 값을 쓰는 쪽은 AWarriorStageGameMode(상태·웨이브)와 AWarriorStageFlowManager(적 수)뿐이고,
 * UI·플레이어·상점은 조회와 구독만 한다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorStageGameState : public AWarriorGameState
{
	GENERATED_BODY()

public:
	AWarriorStageGameState();

	//~ Begin 조회
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	EWarriorStageState GetStageState() const { return StageState; }

	//1부터. 첫 웨이브 시작 전에는 0
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	int32 GetWaveNumber() const { return WaveNumber; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	int32 GetTotalWaveCount() const { return TotalWaveCount; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	bool IsBossWave() const { return bBossWave; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	int32 GetAliveEnemyCount() const { return AliveEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	int32 GetTotalEnemyCount() const { return TotalEnemyCount; }

	//현재 상태의 남은 시간(초). 타이머가 없는 상태는 0
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	float GetStateRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	FWarriorStagePermission GetPermission(EWarriorStageState InState) const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	bool IsActionAllowedInCurrentState(EWarriorStageAction InAction) const;

	//스테이지 GameState가 없는 레벨(전투 테스트 맵 등)에서는 항상 true
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage", meta = (WorldContext = "WorldContextObject"))
	static bool IsActionAllowed(const UObject* WorldContextObject, EWarriorStageAction InAction);
	//~ End 조회

	//~ Begin 쓰기 (GameMode·FlowManager 전용)
	//InDuration이 0보다 크면 남은 시간 계산에 쓰인다. 같은 상태로는 바꾸지 않는다.
	void SetStageState(EWarriorStageState InNewState, float InDuration);

	void SetWaveInfo(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave);

	void SetEnemyCount(int32 InAliveCount, int32 InTotalCount);
	//~ End 쓰기

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorStageStateChanged OnStageStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorWaveChanged OnWaveChanged;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorEnemyCountChanged OnEnemyCountChanged;

protected:
	//상태별 허용 조작. 생성자에서 기본 표를 채우고, BP 파생에서 덮어쓸 수 있다.
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TMap<EWarriorStageState, FWarriorStagePermission> StatePermissions;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Warrior|Stage")
	EWarriorStageState StageState = EWarriorStageState::None;

	UPROPERTY(VisibleInstanceOnly, Category = "Warrior|Stage")
	int32 WaveNumber = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Warrior|Stage")
	int32 TotalWaveCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Warrior|Stage")
	bool bBossWave = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Warrior|Stage")
	int32 AliveEnemyCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Warrior|Stage")
	int32 TotalEnemyCount = 0;

	//상태가 끝나는 서버 월드 시간. 타이머가 없으면 0
	float StateEndTime = 0.f;
};
