// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameMode.h"
#include "WarriorFrontGameMode.generated.h"

/**
 * 타이틀·메인메뉴 레벨(L_Front)용 GameMode.
 * 플레이어 폰을 스폰하지 않고, 화면 전환은 AWarriorFrontPlayerController가 맡는다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorFrontGameMode : public AWarriorGameMode
{
	GENERATED_BODY()

public:
	AWarriorFrontGameMode();
};
