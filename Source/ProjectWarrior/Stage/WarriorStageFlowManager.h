// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageFlowManager.generated.h"

class UWarriorStageWaveDataAsset;
class AWarriorWaveSpawner;

/**
 * 스테이지 레벨에 1개 배치하는 웨이브 실행기. 흐름(다음 웨이브, 휴식, 클리어, 실패)은 판단하지 않고
 * AWarriorStageGameMode의 명령을 수행한 뒤 결과만 보고한다.
 *
 * GameMode와의 이음매. "GameMode가 호출" 구역의 이름·인자·반환형은 바꾸지 않는다.
 * bUseDebugWaves가 false이면 DataAsset의 웨이브를 WaveSpawner로 실행한다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorStageFlowManager : public AActor
{
	GENERATED_BODY()

public:
	AWarriorStageFlowManager();

	//~ Begin GameMode가 호출 (시그니처 고정)
	//InWaveIndex는 0부터. 시작하지 못하면 false
	bool StartWave(int32 InWaveIndex);

	//남은 스폰 요청을 취소한다. 클리어·실패 시 호출
	void StopAll();

	int32 GetTotalWaveCount() const;

	bool IsLastWave(int32 InWaveIndex) const;

	bool IsBossWave(int32 InWaveIndex) const;

	//0이면 GameMode의 기본 쉬는 시간을 쓴다
	float GetRestTimeOverride(int32 InWaveIndex) const;
	//~ End GameMode가 호출

	//현재 웨이브의 적이 전멸했고 대기 스폰도 없음. 직접 Broadcast 하지 말고 ReportWaveCleared를 쓴다.
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorWaveCleared OnWaveCleared;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//남은 적 수(생존 + 미스폰)와 계획된 전체 적 수를 GameState에 알린다 (HUD가 읽음)
	void ReportEnemyCount(int32 InAliveCount, int32 InTotalCount);

	//OnWaveCleared를 방송한다. 같은 웨이브 번호로는 한 번만 방송한다. InWaveNumber는 1부터
	void ReportWaveCleared(int32 InWaveNumber);

	//~ Begin 디버그 웨이브 (실제 구현이 붙으면 레벨에서 false로)
	//true면 적을 스폰하지 않고 DebugWaveClearTime 뒤에 클리어로 처리한다
	UPROPERTY(EditAnywhere, Category = "Warrior|Stage|Debug")
	bool bUseDebugWaves = true;

	UPROPERTY(EditAnywhere, Category = "Warrior|Stage|Debug", meta = (EditCondition = "bUseDebugWaves", ClampMin = "1"))
	int32 DebugWaveCount = 5;

	UPROPERTY(EditAnywhere, Category = "Warrior|Stage|Debug", meta = (EditCondition = "bUseDebugWaves", ClampMin = "0.1"))
	float DebugWaveClearTime = 5.f;

	//디버그 웨이브에서 보고할 가짜 적 수
	UPROPERTY(EditAnywhere, Category = "Warrior|Stage|Debug", meta = (EditCondition = "bUseDebugWaves", ClampMin = "0"))
	int32 DebugEnemyCount = 3;
	//~ End 디버그 웨이브

	UPROPERTY(EditAnywhere, Category = "Warrior|Stage")
	TObjectPtr<UWarriorStageWaveDataAsset> StageWaveData;

	UPROPERTY(EditAnywhere, Category = "Warrior|Stage")
	TObjectPtr<AWarriorWaveSpawner> WaveSpawner;

private:
	void HandleDebugWaveTimerElapsed();
	void HandleSpawnerEnemyCountChanged(int32 InAliveCount, int32 InSpawnedCount);
	void HandleSpawnerWaveCleared();

	//현재 웨이브 데이터에 설정된 적 수량의 합
	int32 PlannedEnemyCount = 0;

	FTimerHandle DebugWaveTimerHandle;

	//진행 중인 웨이브 번호(1부터). 없으면 0
	int32 ActiveWaveNumber = 0;

	//마지막으로 OnWaveCleared를 방송한 웨이브 번호. 중복 방송 방지용
	int32 LastClearedWaveNumber = 0;
};
