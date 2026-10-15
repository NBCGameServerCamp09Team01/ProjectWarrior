// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameMode.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"
#include "WarriorStageGameMode.generated.h"

class AWarriorStageFlowManager;
class UWarriorStageWaveDataAsset;
struct FWarriorApiResult;
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

	//이번 판이 체크포인트에서 이어 가는 판인가(M22). true면 준비 시간 대신 다음 웨이브의 쉬는 시간부터 시작한다
	bool IsResumingFromCheckpoint() const { return bResumingFromCheckpoint; }

	//InitGame에서 정한 스테이지 키. 판 끝까지 바뀌지 않는다. 결과·판 시작·체크포인트에 실린다
	FName GetResolvedStageId() const { return ResolvedLaunchInfo.StageId; }

	//~ Begin 상태 객체 전용
	//웨이브 인덱스를 1 올리고 GameState에 기록한 뒤 FlowManager에 시작을 요청한다. 시작하지 못하면 그 웨이브를 건너뛴다.
	void StartNextWave();

	//FlowManager가 없으면 true (진행할 웨이브가 없으므로 클리어로 처리)
	bool IsCurrentWaveLast() const;

	//다음 웨이브의 RestTimeOverride가 0보다 크면 그 값, 아니면 RestTime
	float GetNextRestTime() const;

	void StopAllWaves();

	//방금 클리어한 웨이브의 체크포인트를 서버에 올린다(M22, C1). 마지막 웨이브면 올리지 않는다(결과를 낸다).
	//실패해도 판은 그대로 진행하고 보관하지 않는다(다음 웨이브 클리어 때 새로 올림)
	void UploadWaveCheckpoint();

	//웨이브를 시작해도 되는가(M24): 서버 웨이브 구성을 FlowManager에 넣었거나, 서버 플레이가 없는 판이라 로컬 웨이브로 대신함
	bool CanStartWaves() const { return bWavePlanApplied || bUsingLocalWaves; }

	//준비 시간이 끝났는데 구성이 아직 없다(M24). UWarriorStageSettings::ServerWaveWaitSeconds까지 기다리고, 넘으면 판을 시작하지 않는다
	void WaitForWavePlan();
	//~ End 상태 객체 전용

	// Stage종료 후 StageUpgrade 원상복구.
	void ResetStageUpgrades();

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin AGameModeBase Interface.
	//레벨 액터의 BeginPlay보다 먼저 불린다. 여기서 이 판의 스테이지를 정한다(FlowManager가 GameMode보다 먼저 등록될 수 있음).
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
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

	//선택 화면을 거치지 않고 이 레벨을 열었을 때 쓸 스테이지 키(서버 stageId, 예: stage.01.01).
	//비어 있으면 스테이지 목록에서 이 레벨의 첫 스테이지, 그것도 없으면 레벨 이름을 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	FName StageId;

	//결과에 기록되는 난이도(M9). 레벨 시작 때 선택 화면 값(실행 정보)으로, 서버 구성이 들어오면 그 판의 난이도(응답 difficulty)로 바뀐다.
	//선택 화면을 거치지 않고 레벨을 열 때의 기본값으로만 이 칸을 쓴다. 적 능력치 배율은 서버 구성이 적 줄마다 준다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage", meta = (ClampMin = "0"))
	int32 Difficulty = 0;

private:
	void CreateStates();

	void ChangeState(EWarriorStageState InNewState);

	//Initializing 상태에서 FlowManager 등록과 플레이어 스폰이 모두 끝났으면 StageReady를 보낸다.
	//순서가 보장되지 않아 BeginPlay, RestartPlayer, RegisterStageFlowManager에서 모두 호출한다.
	void TryFinishInitialize();

	void HandleStateTimerElapsed();

	void ProcessPendingEvents();

	//한 판 시작. RunId를 정해 GameState에 기록한다. 스테이지가 준비된 순간(StageReady 직전)에 한 번 호출된다.
	void BeginRun();

	//한 판 끝. 결과를 확정해 GameState에 넘긴다. 끝 상태로 바뀌기 직전에 한 번 호출된다.
	void FinishRun(bool bInCleared);

	//이 판의 스테이지를 정한다. 순서: 선택 화면의 실행 정보 → StageId 칸 → 목록에서 이 레벨의 첫 스테이지 → 레벨 이름
	void ResolveStageLaunch();

	//개발용 로컬 웨이브(스테이지 목록 → 레벨 FlowManager 순, 둘 다 에디터 전용)를 FlowManager에 넣는다.
	//M24: 서버 플레이가 없는 판에서만 HandleWavePlanChanged가 부른다. 서버 판은 로컬 구성을 읽지 않는다
	void ApplyStageWaveData(AWarriorStageFlowManager* InFlowManager) const;

	//~ Begin 서버 웨이브 구성(M24, stage-play-api.md v1.3 "웨이브 구성")
	//UWarriorStagePlaySubsystem::OnWavePlanChanged. BeginRun 전에는 무시한다(지난 판의 늦은 알림). 여러 번 불러도 같다
	void HandleWavePlanChanged();

	//서버 구성을 임시 웨이브 에셋에 담아 FlowManager에 넣고, 총 웨이브 수를 GameState에 알린다
	void ApplyServerWavePlan(const TArray<FWarriorStageWaveData>& InWaves, const FString& InWaveVersion);

	//기다림 한도를 넘음 → STAGE_WAVE_PLAN_TIMEOUT
	void HandleWavePlanTimeout();

	//판을 시작하지 않는다: 웨이브를 멈추고, 쓸 수 없는 서버 판은 포기(P4)하고, 안내 팝업을 띄운 뒤 메인메뉴로 간다(결과는 내지 않음)
	void FailRunStart(const FWarriorApiResult& InError);

	//서버 구성을 담는 임시 웨이브 에셋(저장하지 않음)
	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageWaveDataAsset> ServerWaveData;

	FDelegateHandle WavePlanChangedHandle;
	FTimerHandle WavePlanWaitHandle;

	bool bWavePlanApplied = false;
	bool bUsingLocalWaves = false;
	bool bWaitingForWavePlan = false;
	bool bRunStartFailed = false;
	//~ End 서버 웨이브 구성

	FWarriorStageLaunchInfo ResolvedLaunchInfo;

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

	FGuid RunId;

	//BeginRun 시점의 실제 시간(초). 플레이 시간 계산에 쓴다.
	//체크포인트에서 이어 가면 그때까지의 플레이 시간만큼 앞으로 당긴다(이어한 판의 시간 = 체크포인트 시간 + 이어한 뒤, M22)
	double RunStartRealTime = 0.0;

	//이번 판의 플레이 시간(초). 결과 제출과 체크포인트에 같은 값을 쓴다
	float GetRunPlayTimeSeconds() const;

	bool bRunFinished = false;

	//체크포인트에서 이어 가는 판(BeginRun에서 정함)
	bool bResumingFromCheckpoint = false;

private:
	//체크포인트 값을 넣고 웨이브 위치를 클리어한 웨이브로 맞춘다. 넣을 수 없으면 false(처음부터 시작)
	bool ApplyResumeCheckpoint(const FWarriorPlayCheckpoint& InCheckpoint);
};
