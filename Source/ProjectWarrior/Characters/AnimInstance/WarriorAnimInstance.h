// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Animation/ALSCharacterAnimInstance.h"
#include "WarriorAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorAnimInstance : public UALSCharacterAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;

	virtual void NativeBeginPlay() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:

};
