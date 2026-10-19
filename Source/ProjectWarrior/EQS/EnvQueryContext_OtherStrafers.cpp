// Fill out your copyright notice in the Description page of Project Settings.


#include "EnvQueryContext_OtherStrafers.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "ProjectWarrior/Controllers/WarriorAIController.h"
#include "BrainComponent.h"
#include "EngineUtils.h"

void UEnvQueryContext_OtherStrafers::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const APawn* QuerierPawn = Cast<APawn>(QueryInstance.Owner.Get());
	const AAIController* QuerierController = QuerierPawn ? Cast<AAIController>(QuerierPawn->GetController()) : nullptr;
	const UBlackboardComponent* QuerierBlackboard = QuerierController ? QuerierController->GetBlackboardComponent() : nullptr;
	const UObject* QuerierTarget = QuerierBlackboard ? QuerierBlackboard->GetValueAsObject(TargetActorKeyName) : nullptr;
	UWorld* World = QuerierPawn ? QuerierPawn->GetWorld() : nullptr;

	TArray<FVector> Locations;

	if (World && QuerierTarget)
	{
		for (TActorIterator<AWarriorAIController> It(World); It; ++It)
		{
			const AWarriorAIController* OtherController = *It;
			const APawn* OtherPawn = OtherController->GetPawn();
			const UBlackboardComponent* OtherBlackboard = OtherController->GetBlackboardComponent();

			if (OtherController == QuerierController || !OtherPawn || !OtherBlackboard)
			{
				continue;
			}

			// 같은 대상을 노리는 AI만 (사망하면 BT가 멈추지만 블랙보드는 남으므로 BrainComponent 실행 여부도 확인)
			if (OtherBlackboard->GetValueAsObject(TargetActorKeyName) != QuerierTarget || !OtherController->GetBrainComponent() || !OtherController->GetBrainComponent()->IsRunning())
			{
				continue;
			}

			Locations.Add(OtherPawn->GetActorLocation());

			if (OtherBlackboard->IsVectorValueSet(StrafeLocationKeyName))
			{
				Locations.Add(OtherBlackboard->GetValueAsVector(StrafeLocationKeyName));
			}
		}
	}

	UEnvQueryItemType_Point::SetContextHelper(ContextData, Locations);
}
