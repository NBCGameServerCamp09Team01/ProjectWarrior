// Fill out your copyright notice in the Description page of Project Settings.


#include "BTDecorator_AttackToken.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "ProjectWarrior/Components/Combat/AttackTokenComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

UBTDecorator_AttackToken::UBTDecorator_AttackToken()
{
	NodeName = "Attack Token";

	bNotifyActivation = true;
	bNotifyDeactivation = true;
	bNotifyTick = true;
	bNotifyBecomeRelevant = true;

	FlowAbortMode = EBTFlowAbortMode::Both;

	TargetActorKey.SelectedKeyName = FName("TargetActor");
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_AttackToken, TargetActorKey), AActor::StaticClass());

	TokenPool = WarriorGameplayTags::AI_AttackToken_Melee;
}

void UBTDecorator_AttackToken::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

uint16 UBTDecorator_AttackToken::GetInstanceMemorySize() const
{
	return sizeof(FAttackTokenDecoratorMemory);
}

void UBTDecorator_AttackToken::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	InitializeNodeMemory<FAttackTokenDecoratorMemory>(NodeMemory, InitType);
}

void UBTDecorator_AttackToken::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	// BT가 통째로 정리될 때(사망, 컨트롤러 해제) 들고 있던 토큰 반납
	ReleaseToken(OwnerComp, *CastInstanceNodeMemory<FAttackTokenDecoratorMemory>(NodeMemory));

	CleanupNodeMemory<FAttackTokenDecoratorMemory>(NodeMemory, CleanupType);
}

FString UBTDecorator_AttackToken::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s x%d"), *Super::GetStaticDescription(), *TokenPool.ToString(), TokenCost);
}

bool UBTDecorator_AttackToken::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UAttackTokenComponent* TokenComponent = FindTokenComponent(OwnerComp);
	const AAIController* AIController = OwnerComp.GetAIOwner();

	// 대상이 토큰을 쓰지 않으면 제한하지 않음
	if (!TokenComponent || !AIController)
	{
		return true;
	}

	// 이미 들고 있으면 true
	return TokenComponent->CanAcquire(AIController->GetPawn(), TokenPool, TokenCost);
}

void UBTDecorator_AttackToken::OnNodeActivation(FBehaviorTreeSearchData& SearchData)
{
	FAttackTokenDecoratorMemory* Memory = GetNodeMemory<FAttackTokenDecoratorMemory>(SearchData);
	Memory->AcquiredFrom.Reset();
	Memory->TimeUntilCheck = CheckInterval;
	// 분기에 들어왔다는 것은 조건이 통과했다는 뜻. 획득에 실패했으면 다음 확인에서 false로 바뀌어 분기를 중단
	Memory->bLastCondition = true;

	UAttackTokenComponent* TokenComponent = FindTokenComponent(SearchData.OwnerComp);
	const AAIController* AIController = SearchData.OwnerComp.GetAIOwner();

	if (TokenComponent && AIController && TokenComponent->TryAcquire(AIController->GetPawn(), TokenPool, TokenCost))
	{
		Memory->AcquiredFrom = TokenComponent;
	}
}

void UBTDecorator_AttackToken::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	// 감시를 시작한 시점(스트레이프 시작 등)의 결과를 기준으로 삼음
	FAttackTokenDecoratorMemory* Memory = CastInstanceNodeMemory<FAttackTokenDecoratorMemory>(NodeMemory);
	Memory->bLastCondition = CalculateRawConditionValue(OwnerComp, NodeMemory);
	Memory->TimeUntilCheck = CheckInterval;
}

void UBTDecorator_AttackToken::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
	ReleaseToken(SearchData.OwnerComp, *GetNodeMemory<FAttackTokenDecoratorMemory>(SearchData));
}

void UBTDecorator_AttackToken::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FAttackTokenDecoratorMemory* Memory = CastInstanceNodeMemory<FAttackTokenDecoratorMemory>(NodeMemory);

	Memory->TimeUntilCheck -= DeltaSeconds;
	if (Memory->TimeUntilCheck > 0.f)
	{
		return;
	}
	Memory->TimeUntilCheck = CheckInterval;

	// 결과가 바뀐 순간에만 요청. 매번 요청하면 토큰이 남아 있어도 Cooldown 등 다른 조건에 막힐 때 스트레이프가 0.2초마다 처음부터 다시 시작됨
	const bool bCondition = CalculateRawConditionValue(OwnerComp, NodeMemory);
	if (bCondition == Memory->bLastCondition)
	{
		return;
	}
	Memory->bLastCondition = bCondition;

	// 스트레이프 중 토큰이 생겼거나(LowerPriority), 접근 중 토큰을 잃었으면(Self) 분기 전환
	ConditionalFlowAbort(OwnerComp, EBTDecoratorAbortRequest::ConditionResultChanged);
}

UAttackTokenComponent* UBTDecorator_AttackToken::FindTokenComponent(const UBehaviorTreeComponent& OwnerComp) const
{
	const UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	const AActor* TargetActor = BlackboardComponent ? Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

	return TargetActor ? TargetActor->FindComponentByClass<UAttackTokenComponent>() : nullptr;
}

void UBTDecorator_AttackToken::ReleaseToken(UBehaviorTreeComponent& OwnerComp, FAttackTokenDecoratorMemory& Memory) const
{
	if (UAttackTokenComponent* TokenComponent = Memory.AcquiredFrom.Get())
	{
		const AAIController* AIController = OwnerComp.GetAIOwner();
		TokenComponent->Release(AIController ? AIController->GetPawn() : nullptr);
	}

	Memory.AcquiredFrom.Reset();
}
