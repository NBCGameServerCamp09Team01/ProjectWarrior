// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageSettings.h"
#include "WarriorStageCatalogDataAsset.h"

UWarriorStageCatalogDataAsset* UWarriorStageSettings::LoadStageCatalog()
{
	const UWarriorStageSettings* Settings = GetDefault<UWarriorStageSettings>();
	return Settings ? Settings->StageCatalog.LoadSynchronous() : nullptr;
}
