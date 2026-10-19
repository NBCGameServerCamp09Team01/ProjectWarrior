// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WarriorGameInstance.generated.h"

/**
 * 게임 실행 동안 유지되는 GameInstance. 레벨을 옮겨도(메인메뉴 ↔ 스테이지) 사라지지 않는다.
 *
 * 본체는 얇게 둔다. 판과 판 사이에 유지되는 데이터와 기능은 담당별 GameInstanceSubsystem이 맡는다.
 * - UWarriorProfileStatsSubsystem: 통계와 기록
 * - UWarriorStageLaunchSubsystem: 다음에 플레이할 스테이지 선택값과 실행 정보 (10/9 이 클래스에서 옮김)
 * - 계정 서브시스템: 계정 레벨, 스탯 포인트, 투자 내역 (예정)
 * - 웹 통신 서브시스템: 서버 연동 (예정)
 * 새 기능은 이 클래스에 넣지 말고 서브시스템으로 추가한다. 여러 명이 같은 파일을 동시에 고치지 않기 위해서다.
 *
 * 본체가 가진 것: 시작·종료 로그.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//월드 컨텍스트로 GameInstance를 얻는다. GameInstance가 이 클래스가 아니면 nullptr
	UFUNCTION(BlueprintPure, Category = "Warrior|GameInstance", meta = (WorldContext = "WorldContextObject"))
	static UWarriorGameInstance* Get(const UObject* WorldContextObject);

protected:
	//~ Begin UGameInstance Interface.
	virtual void Init() override;
	virtual void Shutdown() override;
	//~ End UGameInstance Interface
};
