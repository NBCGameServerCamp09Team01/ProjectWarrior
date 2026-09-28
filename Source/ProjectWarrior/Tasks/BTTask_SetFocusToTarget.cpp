// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_SetFocusToTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "ProjectWarrior/Characters/WarriorBaseCharacter.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "AIController.h"

UBTTask_SetFocusToTarget::UBTTask_SetFocusToTarget()
{
	INIT_TASK_NODE_NOTIFY_FLAGS();
	NodeName = "Set Focus To Target";
	//bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_SetFocusToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AWarriorBaseCharacter* TargetCharacter = Cast<AWarriorBaseCharacter>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(GetSelectedBlackboardKey()));

	if (TargetCharacter)
	{
		OwnerComp.GetAIOwner()->SetFocus(TargetCharacter);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

void UBTTask_SetFocusToTarget::FinishLatentTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type TaskResult) const
{
}

FString UBTTask_SetFocusToTarget::GetStaticDescription() const
{
	return "Set FocusTarget";
}
