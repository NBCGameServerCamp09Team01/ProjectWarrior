// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnCombatComponent.h"
#include "AICombatComponent.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UAICombatComponent : public UPawnCombatComponent
{
	GENERATED_BODY()

public:
	virtual void OnHitTargetActor(AActor* HitActor) override;
};
