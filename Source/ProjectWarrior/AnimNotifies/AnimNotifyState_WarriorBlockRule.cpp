// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotifyState_WarriorBlockRule.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/Components/Combat/PawnCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UPawnCombatComponent* GetOwnerCombatComponent(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;

		// 애니메이션 에디터 미리 보기에는 전투 컴포넌트가 없음
		return Owner ? UWarriorFunctionLibrary::NativeGetPawnCombatComponentFromActor(Owner) : nullptr;
	}
}

FString UAnimNotifyState_WarriorBlockRule::GetNotifyName_Implementation() const
{
	FString RuleName;
	switch (BlockRule)
	{
	case EWarriorBlockRule::PerfectParryOnly:	RuleName = TEXT("PerfectParryOnly");	break;
	case EWarriorBlockRule::Unblockable:		RuleName = TEXT("Unblockable");			break;
	default:									RuleName = TEXT("Blockable");			break;
	}

	return bStaggerOnBlocked
		? FString::Printf(TEXT("Block: %s (Stagger)"), *RuleName)
		: FString::Printf(TEXT("Block: %s"), *RuleName);
}

void UAnimNotifyState_WarriorBlockRule::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UPawnCombatComponent* CombatComponent = GetOwnerCombatComponent(MeshComp))
	{
		CombatComponent->SetCurrentBlockRule(BlockRule, bStaggerOnBlocked);
	}
}

void UAnimNotifyState_WarriorBlockRule::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UPawnCombatComponent* CombatComponent = GetOwnerCombatComponent(MeshComp))
	{
		CombatComponent->ResetCurrentBlockRule();
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
