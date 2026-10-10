// Fill out your copyright notice in the Description page of Project Settings.


#include "BTS_UpdateBossTargetInfo.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "ProjectWarrior/Characters/WarriorBossCharacter.h"
#include "ProjectWarrior/Components/Combat/BossPatternComponent.h"

UBTS_UpdateBossTargetInfo::UBTS_UpdateBossTargetInfo()
{
	NodeName = TEXT("Native Update Boss Target Info");

	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	Interval = 0.1f;
	RandomDeviation = 0.f;

	// 분기에 들어오는 즉시 기록 (첫 틱 전에 패턴이 끝나 Wait로 넘어가도 값이 들어 있게)
	bCallTickOnSearchStart = true;

	InTargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, InTargetActorKey), AActor::StaticClass());
	OutDistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, OutDistanceKey));
	OutAngleKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, OutAngleKey));
	OutPatternRecoveryTimeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, OutPatternRecoveryTimeKey));

	// 키를 지정하지 않은 기존 노드는 그대로 동작하도록 기본은 비움
	OutPatternRecoveryTimeKey.SelectedKeyName = NAME_None;
}

void UBTS_UpdateBossTargetInfo::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InTargetActorKey.ResolveSelectedKey(*BBAsset);
		OutDistanceKey.ResolveSelectedKey(*BBAsset);
		OutAngleKey.ResolveSelectedKey(*BBAsset);
		OutPatternRecoveryTimeKey.ResolveSelectedKey(*BBAsset);
	}
}

FString UBTS_UpdateBossTargetInfo::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s -> Distance: %s, Angle: %s %s"),
		*InTargetActorKey.SelectedKeyName.ToString(),
		*OutDistanceKey.SelectedKeyName.ToString(),
		*OutAngleKey.SelectedKeyName.ToString(),
		*GetStaticServiceDescription());
}

void UBTS_UpdateBossTargetInfo::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	AWarriorBossCharacter* BossCharacter = AIController ? Cast<AWarriorBossCharacter>(AIController->GetPawn()) : nullptr;

	if (!BlackboardComponent || !BossCharacter || !BossCharacter->GetBossPatternComponent())
	{
		return;
	}

	// 대상과 무관하게 기록 (대상이 없어도 Wait 시간은 맞아야 함)
	if (OutPatternRecoveryTimeKey.IsSet())
	{
		BlackboardComponent->SetValueAsFloat(OutPatternRecoveryTimeKey.SelectedKeyName, BossCharacter->GetPatternRecoveryTime());
	}

	AActor* TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(InTargetActorKey.SelectedKeyName));

	float Distance = 0.f;
	float Angle = 0.f;

	if (!BossCharacter->GetBossPatternComponent()->GetTargetDistanceAndAngle(TargetActor, Distance, Angle))
	{
		return;
	}

	if (OutDistanceKey.IsSet())
	{
		BlackboardComponent->SetValueAsFloat(OutDistanceKey.SelectedKeyName, Distance);
	}

	if (OutAngleKey.IsSet())
	{
		BlackboardComponent->SetValueAsFloat(OutAngleKey.SelectedKeyName, Angle);
	}
}
