// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_OrientToTargetActor.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UBTS_OrientToTargetActor : public UBTService
{
	GENERATED_BODY()
	
	UBTS_OrientToTargetActor();

	// Begin UBTNode Interface
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual FString GetStaticDescription() const override;

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// End UBTNode Interface

	UPROPERTY(EditAnywhere, Category="Target")
	FBlackboardKeySelector InTargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Target")
	float RotationInterpSpeed;
};
