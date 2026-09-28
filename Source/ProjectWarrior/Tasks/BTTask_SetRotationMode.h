// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "BTTask_SetRotationMode.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UBTTask_SetRotationMode : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_SetRotationMode();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);

	UPROPERTY(EditAnywhere)
	EALSRotationMode NewRotationMode;
};
