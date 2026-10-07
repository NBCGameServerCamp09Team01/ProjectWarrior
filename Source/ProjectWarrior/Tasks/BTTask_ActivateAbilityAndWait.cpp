// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_ActivateAbilityAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_ActivateAbilityAndWait::UBTTask_ActivateAbilityAndWait()
{
	NodeName = "Activate Ability And Wait";

	// 대기 중인 어빌리티와 델리게이트를 인스턴스 멤버로 들고 있음
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_ActivateAbilityAndWait::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	UAbilitySystemComponent* ASC = AIController ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AIController->GetPawn()) : nullptr;

	if (!ASC || !AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	TArray<FGameplayAbilitySpec*> Specs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTag.GetSingleTagContainer(), Specs);
	Specs.RemoveAll([](const FGameplayAbilitySpec* Spec) { return !Spec || Spec->IsActive(); });

	if (Specs.IsEmpty())
	{
		return EBTNodeResult::Failed;
	}

	const FGameplayAbilitySpecHandle SpecHandle = Specs[FMath::RandRange(0, Specs.Num() - 1)]->Handle;

	// 발동 중에 바로 끝나는 경우도 받도록 먼저 구독
	bAbortPending = false;
	CachedOwnerComp = &OwnerComp;
	CachedASC = ASC;
	ActiveSpecHandle = SpecHandle;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);

	bActivating = true;
	const bool bActivated = ASC->TryActivateAbility(SpecHandle);
	bActivating = false;

	if (!bActivated)
	{
		StopListening();
		return EBTNodeResult::Failed;
	}

	// 발동 즉시 끝났으면 HandleAbilityEnded가 이미 구독을 끊었음
	if (!AbilityEndedHandle.IsValid())
	{
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ActivateAbilityAndWait::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 어빌리티가 아직 진행 중이면 끝날 때까지 중단을 미룸 (HandleAbilityEnded에서 FinishLatentAbort)
	if (bFinishAbilityBeforeAbort && AbilityEndedHandle.IsValid())
	{
		bAbortPending = true;
		return EBTNodeResult::InProgress;
	}

	UAbilitySystemComponent* ASC = CachedASC.Get();
	const FGameplayAbilitySpecHandle SpecHandle = ActiveSpecHandle;
	StopListening();

	if (bCancelAbilityOnAbort && ASC && SpecHandle.IsValid())
	{
		ASC->CancelAbilityHandle(SpecHandle);
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_ActivateAbilityAndWait::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	StopListening();

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_ActivateAbilityAndWait::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\nActivate %s and wait%s"), *Super::GetStaticDescription(), *AbilityTag.ToString(),
		bFinishAbilityBeforeAbort ? TEXT("\n(finish before abort)") : TEXT(""));
}

void UBTTask_ActivateAbilityAndWait::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilitySpecHandle != ActiveSpecHandle)
	{
		return;
	}

	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	StopListening();

	if (bAbortPending)
	{
		bAbortPending = false;
		if (OwnerComp)
		{
			FinishLatentAbort(*OwnerComp);
		}
		return;
	}

	// ExecuteTask 안에서 바로 끝난 경우는 ExecuteTask가 결과를 반환함
	if (OwnerComp && !bActivating)
	{
		FinishLatentTask(*OwnerComp, EndedData.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}
}

void UBTTask_ActivateAbilityAndWait::StopListening()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}

	AbilityEndedHandle.Reset();
	ActiveSpecHandle = FGameplayAbilitySpecHandle();
	CachedASC.Reset();
}
