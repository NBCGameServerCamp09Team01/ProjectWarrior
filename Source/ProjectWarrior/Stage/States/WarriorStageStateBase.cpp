// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageStateBase.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"

AWarriorStageGameMode* UWarriorStageStateBase::GetStageGameMode() const
{
	return GetTypedOuter<AWarriorStageGameMode>();
}
