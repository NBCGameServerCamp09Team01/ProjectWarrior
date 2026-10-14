// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WarriorStageSettings.generated.h"

class UWarriorMonsterCatalogDataAsset;
class UWarriorStageCatalogDataAsset;

/**
 * 프로젝트 설정 > Game > Warrior Stage.
 * 게임 전체가 쓸 스테이지 목록을 지정한다. 값은 Config/DefaultGame.ini에 저장된다.
 * 비어 있으면 스테이지 키는 GameMode의 StageId(없으면 레벨 이름)로 정해지고, 웨이브는 레벨의 FlowManager 값을 쓴다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Warrior Stage"))
class PROJECTWARRIOR_API UWarriorStageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//게임 전체가 쓰는 스테이지 목록 (예: /Game/MyGameContents/Stage/DA_StageCatalog)
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	TSoftObjectPtr<UWarriorStageCatalogDataAsset> StageCatalog;

	//몬스터 키 → 적 BP 표(M24, 예: /Game/MyGameContents/Stage/DA_MonsterCatalog). 서버 판 응답의 waves를 웨이브 데이터로 바꿀 때 쓴다
	UPROPERTY(Config, EditAnywhere, Category = "Stage|Server Wave")
	TSoftObjectPtr<UWarriorMonsterCatalogDataAsset> MonsterCatalog;

	//준비 시간이 끝난 뒤 서버 웨이브 구성(판 시작 응답)을 더 기다리는 한도(초). 넘으면 판을 시작하지 않고 메인메뉴(STAGE_WAVE_PLAN_TIMEOUT)
	UPROPERTY(Config, EditAnywhere, Category = "Stage|Server Wave", meta = (ClampMin = "0.0"))
	float ServerWaveWaitSeconds = 10.f;

	//서버 플레이가 없는 판(로그인 없이 연 PIE, 디버그 이어하기)에서 UE 웨이브 DataAsset으로 대신할지. Shipping 빌드에서는 늘 끈다.
	//서버 플레이가 있는 판에는 쓰지 않는다(서버와 다른 적으로 같은 stagePlayId의 결과를 내지 않게)
	UPROPERTY(Config, EditAnywhere, Category = "Stage|Server Wave")
	bool bAllowLocalWaveFallback = true;

	//설정의 스테이지 목록을 읽는다. 아직 로드되지 않았으면 여기서 로드한다
	static UWarriorStageCatalogDataAsset* LoadStageCatalog();

	//서버 플레이가 없는 판에서 로컬 웨이브로 대신해도 되는가(bAllowLocalWaveFallback, Shipping이면 false)
	static bool CanUseLocalWaveFallback();
};
