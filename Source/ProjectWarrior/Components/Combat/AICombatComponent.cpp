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

	//Implement block check
	bool bIsValidBlock = false;
	bool bIsValidDodge = false;
	bool bIsValidHit = true;

	bIsValidHit = bIsValidHit && !UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitActor, WarriorGameplayTags::Shared_Status_Finisher);

	const bool bIsPlayerBlocking = UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitActor, WarriorGameplayTags::Player_Status_Blocking);
	const bool bIsMyAttackUnblockable = false;

	if (bIsPlayerBlocking && !bIsMyAttackUnblockable)
	{
		bIsValidBlock = UWarriorFunctionLibrary::IsValidBlock(GetOwningPawn(), HitActor);
	}

	const bool bIsPlayerDodge = UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitActor, WarriorGameplayTags::Shared_Status_Dodge);

	bIsValidDodge = bIsPlayerDodge;

	FGameplayEventData EventData;
	EventData.Instigator = GetOwningPawn();
	EventData.Target = HitActor;

	if (bIsValidHit)
	{
		if (bIsValidBlock)
		{
			//Handle successful block
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
			(
				HitActor,
				WarriorGameplayTags::Player_Event_Successful_Block,
				EventData
			);
		}
		else if (bIsValidDodge)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
			(
				HitActor,
				WarriorGameplayTags::Player_Event_Successful_Dodge,
				EventData
			);
		}
		else
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
			(
				GetOwningPawn(),
				WarriorGameplayTags::Shared_Event_MeleeHit,
				EventData
			);
		}
	}
}
