// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

AWarriorAICharacter* UAIGameplayAbility::GetAICharacterFromActorInfo()
{
    if (!CachedAICharacter.IsValid())
    {
        CachedAICharacter = Cast<AWarriorAICharacter>(CurrentActorInfo->AvatarActor);
    }

    return CachedAICharacter.IsValid() ? CachedAICharacter.Get() : nullptr;
}

UAICombatComponent* UAIGameplayAbility::GetAICombatComponentFromActorInfo()
{
    return GetAICharacterFromActorInfo()->GetAICombatComponent();
}

FGameplayEffectSpecHandle UAIGameplayAbility::MakeAIDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, const FScalableFloat& InDamageScalableFloat)
{
	check(EffectClass);

	FGameplayEffectContextHandle ContextHandle = GetWarriorAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	ContextHandle.SetAbility(this);
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());
	ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle EffectSpecHandle = GetWarriorAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
		EffectClass,
		GetAbilityLevel(),
		ContextHandle
	);

	EffectSpecHandle.Data->SetSetByCallerMagnitude(
		WarriorGameplayTags::Shared_SetByCaller_BaseDamage,
		InDamageScalableFloat.GetValueAtLevel(GetAbilityLevel())
	);

	return EffectSpecHandle;
}
