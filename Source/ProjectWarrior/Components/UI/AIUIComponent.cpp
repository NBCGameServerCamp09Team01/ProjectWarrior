// Fill out your copyright notice in the Description page of Project Settings.


#include "AIUIComponent.h"

void UAIUIComponent::SetAffixDisplay(const FText& InAffixLine, const FLinearColor& InTierColor)
{
	AffixLine = InAffixLine;
	TierColor = InTierColor;
	OnAffixDisplayChanged.Broadcast(AffixLine, TierColor);
}
