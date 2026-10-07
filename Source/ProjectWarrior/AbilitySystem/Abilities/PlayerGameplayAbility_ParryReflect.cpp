// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_ParryReflect.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

UPlayerGameplayAbility_ParryReflect::UPlayerGameplayAbility_ParryReflect()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer ReflectAssetTags;
	ReflectAssetTags.AddTag(WarriorGameplayTags::Player_Ability_ParryReflect);
	SetAssetTags(ReflectAssetTags);

	// 투사체가 막힌 순간 즉시 실행되어야 막힌 투사체가 파괴되기 전에 되돌릴 수 있음
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = WarriorGameplayTags::Player_Event_Successful_Block;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);
}

void UPlayerGameplayAbility_ParryReflect::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	// 근접 공격을 막은 경우에는 투사체가 없음
	AWarriorProjectileBase* Projectile = TriggerEventData
		? Cast<AWarriorProjectileBase>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()))
		: nullptr;

	if (AvatarPawn && Projectile
		&& UWarriorFunctionLibrary::NativeDoesActorHaveTag(AvatarPawn, WarriorGameplayTags::Player_Status_Blocking_Perfect))
	{
		Projectile->ReflectProjectile(AvatarPawn, DamageMultiplier, SpeedMultiplier);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
