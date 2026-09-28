// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "WarriorPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()
	
public:
	AWarriorPlayerController();

	//~ Begin IGenericTeamAgentInterface Interface.
	virtual FGenericTeamId GetGenericTeamId() const override;
	//~ End IGenericTeamAgentInterface Interface

private:
	FGenericTeamId PlayerTeamID;
};
