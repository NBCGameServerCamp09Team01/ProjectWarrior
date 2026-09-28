// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPlayerController.h"

AWarriorPlayerController::AWarriorPlayerController()
{
    PlayerTeamID = FGenericTeamId(0);
}

FGenericTeamId AWarriorPlayerController::GetGenericTeamId() const
{
    return PlayerTeamID;
}
