// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WarriorInteractableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWarriorInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PROJECTWARRIOR_API IWarriorInteractableInterface
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(APawn* InInteractor) const { return true; }
	virtual void Interact(APawn* InInteractor) = 0;

	virtual void SetInteractionFocus(bool bInFocused) {}
};
