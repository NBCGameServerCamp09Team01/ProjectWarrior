// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_SetRotationMode.h"
#include "ProjectWarrior/Characters/WarriorBaseCharacter.h"
#include "AIController.h"

UBTTask_SetRotationMode::UBTTask_SetRotationMode()
{
	INIT_TASK_NODE_NOTIFY_FLAGS();
	NodeName = "Set RotationMode";
}

EBTNodeResult::Type UBTTask_SetRotationMode::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AWarriorBaseCharacter* OwnerAICharacter = Cast<AWarriorBaseCharacter>(OwnerComp.GetAIOwner()->GetPawn());

	if (OwnerAICharacter)
	{
		if (OwnerAICharacter->GetRotationMode() != NewRotationMode)
		{
			OwnerAICharacter->SetRotationMode(NewRotationMode);
		}

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
