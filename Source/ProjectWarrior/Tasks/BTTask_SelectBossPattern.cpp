// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_SelectBossPattern.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "ProjectWarrior/Characters/WarriorBossCharacter.h"
#include "ProjectWarrior/Components/Combat/BossPatternComponent.h"

UBTTask_SelectBossPattern::UBTTask_SelectBossPattern()
{
	NodeName = TEXT("Native Select Boss Pattern");

	bCreateNodeInstance = false;

	InTargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, InTargetActorKey), AActor::StaticClass());
	OutPatternTagKey.AddNameFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, OutPatternTagKey));
}

void UBTTask_SelectBossPattern::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InTargetActorKey.ResolveSelectedKey(*BBAsset);
		OutPatternTagKey.ResolveSelectedKey(*BBAsset);
	}
}

FString UBTTask_SelectBossPattern::GetStaticDescription() const
{
	return FString::Printf(TEXT("Select boss pattern against %s -> %s"),
		*InTargetActorKey.SelectedKeyName.ToString(),
		*OutPatternTagKey.SelectedKeyName.ToString());
}

EBTNodeResult::Type UBTTask_SelectBossPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	AWarriorBossCharacter* BossCharacter = AIController ? Cast<AWarriorBossCharacter>(AIController->GetPawn()) : nullptr;

	if (!BlackboardComponent || !BossCharacter || !BossCharacter->GetBossPatternComponent())
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(InTargetActorKey.SelectedKeyName));

	FGameplayTag SelectedPatternTag;

	if (BossCharacter->GetBossPatternComponent()->SelectPattern(TargetActor, SelectedPatternTag))
	{
		BlackboardComponent->SetValueAsName(OutPatternTagKey.SelectedKeyName, SelectedPatternTag.GetTagName());
		return EBTNodeResult::Succeeded;
	}

	BlackboardComponent->ClearValue(OutPatternTagKey.SelectedKeyName);
	return EBTNodeResult::Failed;
}
