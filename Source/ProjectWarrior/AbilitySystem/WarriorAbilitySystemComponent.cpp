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

    // 발동한 어빌리티가 그 자리에서 다른 어빌리티를 부여·제거하면(예: 전술 모드가 기술 GA를 부여) 도는 중인 배열이 바뀌어
    // "Array has changed during ranged-for iteration" ensure가 남 → 대상 핸들을 먼저 모은 뒤 발동/취소
    TArray<FGameplayAbilitySpecHandle> MatchingHandles;

    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
        {
            MatchingHandles.Add(AbilitySpec.Handle);
        }
    }

    const bool bToggleable = InputTag.MatchesTag(WarriorGameplayTags::InputTag_Toggleable);

    for (const FGameplayAbilitySpecHandle& Handle : MatchingHandles)
    {
        // 앞의 발동으로 제거됐을 수 있으므로 매번 다시 찾음
        const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle);

        if (!AbilitySpec)
        {
            continue;
        }

        if (bToggleable && AbilitySpec->IsActive())
        {
            CancelAbilityHandle(Handle);
        }
        else
        {
            TryActivateAbility(Handle);
        }
    }
}

void UWarriorAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InputTag)
{
    if (!InputTag.IsValid() || !InputTag.MatchesTag(WarriorGameplayTags::InputTag_MustBeHeld))
    {
        return;
    }

    // 취소로 끝나는 어빌리티가 다른 어빌리티를 부여·제거할 수 있으므로 핸들을 먼저 모음
    TArray<FGameplayAbilitySpecHandle> ActiveHandles;

    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
        if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && AbilitySpec.IsActive())
        {
            ActiveHandles.Add(AbilitySpec.Handle);
        }
    }

    for (const FGameplayAbilitySpecHandle& Handle : ActiveHandles)
    {
        CancelAbilityHandle(Handle);
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
