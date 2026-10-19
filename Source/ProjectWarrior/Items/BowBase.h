// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "BowBase.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API ABowBase : public AWeaponBase
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	ABowBase(const FObjectInitializer& ObjectInitializer);
};
