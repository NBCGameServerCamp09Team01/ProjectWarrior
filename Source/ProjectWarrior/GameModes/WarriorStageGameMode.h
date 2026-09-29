// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameMode.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageGameMode.generated.h"

class AWarriorStageFlowManager;

/**
 * 스테이지 레벨의 GameMode. 스테이지 상태 전환을 결정함.
 * 외부는 상태를 직접 바꾸지 않고 SendStageEvent로 사건만 알린다.
 * 상태 머신 본체(상태 객체, ChangeState, 타이머)는 P2에서 추가한다. 추가시 주석 삭제
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

protected:
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
	TWeakObjectPtr<AWarriorStageFlowManager> StageFlowManager;
};
