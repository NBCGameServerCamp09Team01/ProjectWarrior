// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorFrontGameMode.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

AWarriorFrontGameMode::AWarriorFrontGameMode()
{
	//메뉴 화면에서는 캐릭터가 필요 없다
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AWarriorFrontPlayerController::StaticClass();
}
