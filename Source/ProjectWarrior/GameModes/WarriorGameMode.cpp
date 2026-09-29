// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGameMode.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"

AWarriorGameMode::AWarriorGameMode()
{
	PlayerStateClass = AWarriorPlayerState::StaticClass();
}