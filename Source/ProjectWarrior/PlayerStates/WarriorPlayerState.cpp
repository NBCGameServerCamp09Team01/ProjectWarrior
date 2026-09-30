// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPlayerState.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/Components/Upgrade/StageUpgradeComponent.h"

AWarriorPlayerState::AWarriorPlayerState()
{
	PlayerInventoryComponent = CreateDefaultSubobject<UPlayerInventoryComponent>(TEXT("PlayerInventoryComponent"));

	StageUpgradeComponent = CreateDefaultSubobject<UStageUpgradeComponent>(TEXT("StageUpgradeComponent"));
}