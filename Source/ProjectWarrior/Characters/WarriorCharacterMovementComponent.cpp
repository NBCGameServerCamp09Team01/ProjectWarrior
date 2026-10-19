// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorCharacterMovementComponent.h"

float UWarriorCharacterMovementComponent::GetMaxSpeed() const
{
	const float MaxSpeed = Super::GetMaxSpeed();
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		return MaxSpeed * SpeedScale;
	}
	return MaxSpeed;
}
