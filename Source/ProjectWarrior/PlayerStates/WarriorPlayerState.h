// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "WarriorPlayerState.generated.h"

class UPlayerInventoryComponent;

/**
 *
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AWarriorPlayerState();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UPlayerInventoryComponent* PlayerInventoryComponent;

public:
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UPlayerInventoryComponent* GetPlayerInventoryComponent() const { return PlayerInventoryComponent; }
};