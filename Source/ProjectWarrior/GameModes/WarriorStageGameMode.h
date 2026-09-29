// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameMode.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageGameMode.generated.h"

class AWarriorStageFlowManager;
class UWarriorStageStateBase;

/**
 * 스테이지 레벨의 GameMode. 스테이지 상태 전환을 결정함.
 * 외부는 상태를 직접 바꾸지 않고 SendStageEvent로 사건만 알린다.
 * 상태마다 UWarriorStageStateBase 자식 객체를 1개씩 소유하고, 현재 상태 객체가 이벤트를 받아 다음 상태를 정한다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorStageGameMode : public AWarriorGameMode
{
	GENERATED_BODY()

public:
	AWarriorStageGameMode();

	//스테이지에 사건을 알리는 입구. 스테이지 레벨이 아니면 GetAuthGameMode가 nullptr이라 호출되지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void SendStageEvent(EWarriorStageEvent InEvent);

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	EWarriorStageState GetCurrentStageState() const;

	//AWarriorStageFlowManager::BeginPlay에서 호출. 레벨당 1개만 받는다.
	void RegisterStageFlowManager(AWarriorStageFlowManager* InFlowManager);

	AWarriorStageFlowManager* GetStageFlowManager() const { return StageFlowManager.Get(); }

	float GetPrepareTime() const { return PrepareTime; }
	float GetWaveClearedTime() const { return WaveClearedTime; }
	float GetRestTime() const { return RestTime; }

	//현재 웨이브 인덱스(0부터). 첫 웨이브 시작 전에는 INDEX_NONE
	int32 GetCurrentWaveIndex() const { return CurrentWaveIndex; }

	//~ Begin 상태 객체 전용
	//웨이브 인덱스를 1 올리고 GameState에 기록한 뒤 FlowManager에 시작을 요청한다. 시작하지 못하면 그 웨이브를 건너뛴다.
	void StartNextWave();

	//FlowManager가 없으면 true (진행할 웨이브가 없으므로 클리어로 처리)
	bool IsCurrentWaveLast() const;

	//다음 웨이브의 RestTimeOverride가 0보다 크면 그 값, 아니면 RestTime
	float GetNextRestTime() const;

	void StopAllWaves();
	//~ End 상태 객체 전용

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin AGameModeBase Interface.
	virtual void RestartPlayer(AController* NewPlayer) override;
	//~ End AGameModeBase Interface

	//FlowManager의 OnWaveCleared → AllEnemiesDead 이벤트로 변환
	UFUNCTION()
	void HandleWaveCleared(int32 InWaveNumber);

	//최초 쉬는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float PrepareTime = 10.f;

	//웨이브 종료 연출 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float WaveClearedTime = 2.f;

	//웨이브 사이 쉬는 시간(초). 웨이브 데이터의 RestTimeOverride가 0보다 크면 그 값을 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float RestTime = 20.f;

private:
	void CreateStates();

	void ChangeState(EWarriorStageState InNewState);

	//Initializing 상태에서 FlowManager 등록과 플레이어 스폰이 모두 끝났으면 StageReady를 보낸다.
	//순서가 보장되지 않아 BeginPlay, RestartPlayer, RegisterStageFlowManager에서 모두 호출한다.
	void TryFinishInitialize();

	void HandleStateTimerElapsed();

	void ProcessPendingEvents();

	TWeakObjectPtr<AWarriorStageFlowManager> StageFlowManager;

	UPROPERTY(Transient)
	TMap<EWarriorStageState, TObjectPtr<UWarriorStageStateBase>> States;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageStateBase> CurrentState;

	FTimerHandle StateTimerHandle;

	int32 CurrentWaveIndex = INDEX_NONE;

	//상태를 바꾸는 도중(OnExit·OnEnter 실행 중)에 들어온 이벤트는 모았다가 바꾼 뒤 순서대로 처리한다.
	bool bChangingState = false;

	TArray<EWarriorStageEvent> PendingEvents;
};
