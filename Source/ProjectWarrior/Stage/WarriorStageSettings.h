// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WarriorStageSettings.generated.h"

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

	//설정의 스테이지 목록을 읽는다. 아직 로드되지 않았으면 여기서 로드한다
	static UWarriorStageCatalogDataAsset* LoadStageCatalog();
};
