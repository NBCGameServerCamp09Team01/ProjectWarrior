// Fill out your copyright notice in the Description page of Project Settings.


#include "AICombatComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

void UAICombatComponent::OnHitTargetActor(AActor* HitActor)
{
	if (OverlappedActors.Contains(HitActor))
	{
		return;
	}

	OverlappedActors.AddUnique(HitActor);

	FGameplayEventData EventData;
	EventData.Instigator = GetOwningPawn();
	EventData.Target = HitActor;

	// 무기 충돌 노티파이가 지정한 히트리액션 강도를 전달. UAIGameplayAbility::ResolveHitReactEventTag에서 꺼냄
	if (CurrentHitReactEventTag.IsValid())
	{
		EventData.InstigatorTags.AddTag(CurrentHitReactEventTag);
	}

	switch (UWarriorFunctionLibrary::EvaluateHitResult(GetOwningPawn(), HitActor))
	{
	case EWarriorHitResultType::Blocked:
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
		(
			HitActor,
			WarriorGameplayTags::Player_Event_Successful_Block,
			EventData
		);
		break;

	case EWarriorHitResultType::Dodged:
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
		(
			HitActor,
			WarriorGameplayTags::Player_Event_Successful_Dodge,
			EventData
		);
		break;

	case EWarriorHitResultType::Hit:
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
		(
			GetOwningPawn(),
			WarriorGameplayTags::Shared_Event_MeleeHit,
			EventData
		);
		break;

	default:
		break;
	}
}
