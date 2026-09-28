// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnUIInterface.h"

// Add default functionality here for any IPawnUIInterface functions that are not pure virtual.

UPlayerUIComponent* IPawnUIInterface::GetPlayerUIComponent() const
{
    return nullptr;
}

UAIUIComponent* IPawnUIInterface::GetAIUIComponent() const
{
    return nullptr;
}
