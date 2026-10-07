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

	switch (UWarriorFunctionLibrary::EvaluateHitResult(GetOwningPawn(), HitActor, nullptr, CurrentBlockRule))
	{
	case EWarriorHitResultType::Blocked:
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
		(
			HitActor,
			WarriorGameplayTags::Player_Event_Successful_Block,
			EventData
		);

		// 패링 전용 공격 등을 막아 내면 공격자가 무너짐 (보스 분노 연타 마지막 타격)
		if (bStaggerOnBlocked)
		{
			FGameplayEventData StaggerEventData;
			StaggerEventData.Instigator = HitActor;
			StaggerEventData.Target = GetOwningPawn();

			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
			(
				GetOwningPawn(),
				WarriorGameplayTags::Shared_Event_Stagger,
				StaggerEventData
			);
		}
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
