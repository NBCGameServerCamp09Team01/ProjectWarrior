// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WarriorGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorStageSelectionChanged, FName, StageId, int32, Difficulty);

/**
 * 게임 실행 동안 유지되는 GameInstance. 레벨을 옮겨도(메인메뉴 ↔ 스테이지) 사라지지 않는다.
 *
 * 본체는 얇게 둔다. 판과 판 사이에 유지되는 데이터와 기능은 담당별 GameInstanceSubsystem이 맡는다.
 * - UWarriorProfileStatsSubsystem: 통계와 기록
 * - 계정 서브시스템: 계정 레벨, 스탯 포인트, 투자 내역 (예정)
 * - 웹 통신 서브시스템: 서버 연동 (예정)
 * 새 기능은 이 클래스에 넣지 말고 서브시스템으로 추가한다. 여러 명이 같은 파일을 동시에 고치지 않기 위해서다.
 *
 * 본체가 가진 것: 시작·종료 로그, 다음에 플레이할 스테이지 선택값.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//월드 컨텍스트로 GameInstance를 얻는다. GameInstance가 이 클래스가 아니면 nullptr
	UFUNCTION(BlueprintPure, Category = "Warrior|GameInstance", meta = (WorldContext = "WorldContextObject"))
	static UWarriorGameInstance* Get(const UObject* WorldContextObject);

	//다음에 시작할 스테이지를 정한다. 메인메뉴의 스테이지 선택에서 호출한다.
	//StageId가 비어 있으면 스테이지 GameMode의 설정(없으면 레벨 이름)을 그대로 쓴다.
	UFUNCTION(BlueprintCallable, Category = "Warrior|GameInstance")
	void SelectStage(FName InStageId, int32 InDifficulty = 0);

	UFUNCTION(BlueprintPure, Category = "Warrior|GameInstance")
	FName GetSelectedStageId() const { return SelectedStageId; }

	//난이도별 적 스탯 차이는 아직 없다. 자리만 둔다.
	UFUNCTION(BlueprintPure, Category = "Warrior|GameInstance")
	int32 GetSelectedDifficulty() const { return SelectedDifficulty; }

	UPROPERTY(BlueprintAssignable, Category = "Warrior|GameInstance")
	FOnWarriorStageSelectionChanged OnStageSelectionChanged;

protected:
	//~ Begin UGameInstance Interface.
	virtual void Init() override;
	virtual void Shutdown() override;
	//~ End UGameInstance Interface

private:
	FName SelectedStageId = NAME_None;

	int32 SelectedDifficulty = 0;
};
