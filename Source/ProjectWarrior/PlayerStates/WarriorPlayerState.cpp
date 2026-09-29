// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPlayerState.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"

AWarriorPlayerState::AWarriorPlayerState()
{
	PlayerInventoryComponent = CreateDefaultSubobject<UPlayerInventoryComponent>(TEXT("PlayerInventoryComponent"));
}