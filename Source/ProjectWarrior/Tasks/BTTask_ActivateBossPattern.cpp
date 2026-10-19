// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_ActivateBossPattern.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "ProjectWarrior/Characters/WarriorBossCharacter.h"
#include "ProjectWarrior/Components/Combat/BossPatternComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "Abilities/GameplayAbility.h"

UBTTask_ActivateBossPattern::UBTTask_ActivateBossPattern()
{
	NodeName = TEXT("Native Activate Boss Pattern");

	bNotifyTick = true;
	bNotifyTaskFinished = true;
	bCreateNodeInstance = false;

	INIT_TASK_NODE_NOTIFY_FLAGS();

	InPatternTagKey.AddNameFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, InPatternTagKey));
}

void UBTTask_ActivateBossPattern::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InPatternTagKey.ResolveSelectedKey(*BBAsset);
	}
}

uint16 UBTTask_ActivateBossPattern::GetInstanceMemorySize() const
{
	return sizeof(FActivateBossPatternTaskMemory);
}

void UBTTask_ActivateBossPattern::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	InitializeNodeMemory<FActivateBossPatternTaskMemory>(NodeMemory, InitType);
}

void UBTTask_ActivateBossPattern::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	CleanupNodeMemory<FActivateBossPatternTaskMemory>(NodeMemory, CleanupType);
}

FString UBTTask_ActivateBossPattern::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate boss pattern from %s%s"),
		*InPatternTagKey.SelectedKeyName.ToString(),
		bWaitForAbilityEnd ? TEXT(" and wait until it ends") : TEXT(""));
}

EBTNodeResult::Type UBTTask_ActivateBossPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FActivateBossPatternTaskMemory* Memory = CastInstanceNodeMemory<FActivateBossPatternTaskMemory>(NodeMemory);
	check(Memory);
	Memory->Reset();

	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	AWarriorBossCharacter* BossCharacter = AIController ? Cast<AWarriorBossCharacter>(AIController->GetPawn()) : nullptr;
	UWarriorAbilitySystemComponent* ASC = BossCharacter ? BossCharacter->GetWarriorAbilitySystemComponent() : nullptr;

	if (!BlackboardComponent || !ASC)
	{
		return EBTNodeResult::Failed;
	}

	const FGameplayTag PatternTag = FGameplayTag::RequestGameplayTag(BlackboardComponent->GetValueAsName(InPatternTagKey.SelectedKeyName), false);

	if (bClearPatternKeyOnActivate)
	{
		BlackboardComponent->ClearValue(InPatternTagKey.SelectedKeyName);
	}

	if (!PatternTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	TArray<FGameplayAbilitySpec*> FoundAbilitySpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(PatternTag.GetSingleTagContainer(), FoundAbilitySpecs);

	TArray<FGameplayAbilitySpecHandle> CandidateHandles;

	for (const FGameplayAbilitySpec* AbilitySpec : FoundAbilitySpecs)
	{
		if (AbilitySpec && !AbilitySpec->IsActive())
		{
			CandidateHandles.Add(AbilitySpec->Handle);
		}
	}

	FGameplayAbilitySpecHandle ActivatedSpecHandle;

	for (const FGameplayAbilitySpecHandle& CandidateHandle : CandidateHandles)
	{
		if (ASC->TryActivateAbility(CandidateHandle))
		{
			ActivatedSpecHandle = CandidateHandle;
			break;
		}
	}

	if (!ActivatedSpecHandle.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	if (UBossPatternComponent* PatternComponent = BossCharacter->GetBossPatternComponent())
	{
		PatternComponent->NotifyPatternActivated(PatternTag);
	}

	const FGameplayAbilitySpec* ActivatedSpec = ASC->FindAbilitySpecFromHandle(ActivatedSpecHandle);

	// 즉시 끝나는 어빌리티면 기다릴 필요 없음
	if (!bWaitForAbilityEnd || !ActivatedSpec || !ActivatedSpec->IsActive())
	{
		return EBTNodeResult::Succeeded;
	}

	Memory->AbilitySystemComponent = ASC;
	Memory->ActivatedSpecHandle = ActivatedSpecHandle;

	return EBTNodeResult::InProgress;
}

void UBTTask_ActivateBossPattern::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FActivateBossPatternTaskMemory* Memory = CastInstanceNodeMemory<FActivateBossPatternTaskMemory>(NodeMemory);

	UAbilitySystemComponent* ASC = Memory->AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* ActivatedSpec = ASC ? ASC->FindAbilitySpecFromHandle(Memory->ActivatedSpecHandle) : nullptr;

	if (ActivatedSpec && ActivatedSpec->IsActive())
	{
		return;
	}

	// 후속 패턴(도약 -> 돌진 등)이나 페이즈 전환이 이어지고 있으면 끝날 때까지 다음 패턴을 고르지 않음
	if (ASC && HasActiveBossAbility(*ASC))
	{
		return;
	}

	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

bool UBTTask_ActivateBossPattern::HasActiveBossAbility(const UAbilitySystemComponent& ASC)
{
	for (const FGameplayAbilitySpec& AbilitySpec : ASC.GetActivatableAbilities())
	{
		if (AbilitySpec.IsActive() && AbilitySpec.Ability && AbilitySpec.Ability->GetAssetTags().HasTag(WarriorGameplayTags::AI_Ability_Boss))
		{
			return true;
		}
	}

	return false;
}

EBTNodeResult::Type UBTTask_ActivateBossPattern::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FActivateBossPatternTaskMemory* Memory = CastInstanceNodeMemory<FActivateBossPatternTaskMemory>(NodeMemory);

	if (bCancelAbilityOnAbort)
	{
		if (UAbilitySystemComponent* ASC = Memory->AbilitySystemComponent.Get())
		{
			ASC->CancelAbilityHandle(Memory->ActivatedSpecHandle);
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_ActivateBossPattern::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	CastInstanceNodeMemory<FActivateBossPatternTaskMemory>(NodeMemory)->Reset();

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
