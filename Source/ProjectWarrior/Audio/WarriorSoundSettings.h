// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WarriorSoundSettings.generated.h"

class UWarriorSoundSet;

/**
 * 프로젝트 설정 > Game > Warrior Sound.
 * 게임 전체가 쓸 사운드 표를 지정한다. 값은 Config/DefaultGame.ini에 저장된다.
 * 비어 있으면 소리는 나지 않고, 게임은 그대로 동작한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Warrior Sound"))
class PROJECTWARRIOR_API UWarriorSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//게임 전체가 쓰는 사운드 표 (예: /Game/MyGameContents/Audio/DA_SoundSet)
	UPROPERTY(Config, EditAnywhere, Category = "Sound")
	TSoftObjectPtr<UWarriorSoundSet> SoundSet;

	//설정의 사운드 표를 읽는다. 아직 로드되지 않았으면 여기서 로드한다
	static UWarriorSoundSet* LoadSoundSet();
};
