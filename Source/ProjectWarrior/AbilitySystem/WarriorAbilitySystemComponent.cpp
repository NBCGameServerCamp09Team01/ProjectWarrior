// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/Abilities/WarriorGameplayAbility.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

void UWarriorAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag& InputTag)
{
    if (!InputTag.IsValid())
    {
        return;
    }

    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;

        if (InputTag.MatchesTag(WarriorGameplayTags::InputTag_Toggleable))
        {
            if (AbilitySpec.IsActive())
            {
                CancelAbilityHandle(AbilitySpec.Handle);
            }
            else
            {
                TryActivateAbility(AbilitySpec.Handle);
            }
        }
        else
        {
            TryActivateAbility(AbilitySpec.Handle);
        }
    }
}

void UWarriorAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InputTag)
{
    if (!InputTag.IsValid() || !InputTag.MatchesTag(WarriorGameplayTags::InputTag_MustBeHeld))
    {
        return;
    }

    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && AbilitySpec.IsActive())
        {
            CancelAbilityHandle(AbilitySpec.Handle);
        }
    }
}

void UWarriorAbilitySystemComponent::GrantPlayerWeaponAbilities(const TArray<FWarriorPlayerAbilitySet>& InDefaultWeaponAbilities, int32 ApplyLevel, TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilitySpecHandles)
{
    if (InDefaultWeaponAbilities.IsEmpty())
    {
        return;
    }

    for (const FWarriorPlayerAbilitySet& AbilitySet : InDefaultWeaponAbilities)
    {
        if (!AbilitySet.IsValid()) continue;

        FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
        AbilitySpec.SourceObject = GetAvatarActor();
        AbilitySpec.Level = ApplyLevel;
        AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);

        OutGrantedAbilitySpecHandles.AddUnique(GiveAbility(AbilitySpec));
    }
}

void UWarriorAbilitySystemComponent::RemovedGrantedHeroWeaponAbilities(UPARAM(ref)TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove)
{
    if (InSpecHandlesToRemove.IsEmpty())
    {
        return;
    }

    for (const FGameplayAbilitySpecHandle& SpecHandle : InSpecHandlesToRemove)
    {
        if (SpecHandle.IsValid())
        {
            ClearAbility(SpecHandle);
        }
    }

    InSpecHandlesToRemove.Empty();
}

bool UWarriorAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate)
{
    check(AbilityTagToActivate.IsValid());

    TArray<FGameplayAbilitySpec*> FoundAbilitySpecs;
    GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTagToActivate.GetSingleTagContainer(), FoundAbilitySpecs);

    if (!FoundAbilitySpecs.IsEmpty())
    {
        const int32 RandomAbilityIndex = FMath::RandRange(0, FoundAbilitySpecs.Num() - 1);
        FGameplayAbilitySpec* SpecToActivate = FoundAbilitySpecs[RandomAbilityIndex];

        check(SpecToActivate);

        if (!SpecToActivate->IsActive())
        {
            return TryActivateAbility(SpecToActivate->Handle);
        }
    }

    return false;
}
