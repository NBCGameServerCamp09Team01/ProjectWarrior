// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorWidgetBase.h"
#include "ProjectWarrior/Interfaces/PawnUIInterface.h"
#include "ProjectWarrior/Components/UI/PlayerUIComponent.h"
#include "ProjectWarrior/Components/UI/AIUIComponent.h"

void UWarriorWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(GetOwningPlayerPawn()))
	{
		if (UPlayerUIComponent* PlayerUIComponent = PawnUIInterface->GetPlayerUIComponent())
		{
			BP_OnOwningPlayerUIComponentInitialized(PlayerUIComponent);
		}
	}
}

void UWarriorWidgetBase::InitAICreatedWidget(AActor* OwningAIActor)
{
	if (IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(OwningAIActor))
	{
		UAIUIComponent* AIUIComponent = PawnUIInterface->GetAIUIComponent();

		checkf(AIUIComponent, TEXT("Failed to extrac an AIUIComponent from %s"), *OwningAIActor->GetActorNameOrLabel());

		BP_OnOwningAIUIComponentInitialized(AIUIComponent);
	}
}
