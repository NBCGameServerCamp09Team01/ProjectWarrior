// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSoundSettings.h"
#include "WarriorSoundSet.h"

UWarriorSoundSet* UWarriorSoundSettings::LoadSoundSet()
{
	const UWarriorSoundSettings* Settings = GetDefault<UWarriorSoundSettings>();
	return Settings ? Settings->SoundSet.LoadSynchronous() : nullptr;
}
