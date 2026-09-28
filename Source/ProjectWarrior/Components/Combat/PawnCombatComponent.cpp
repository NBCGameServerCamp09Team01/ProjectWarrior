// Fill out your copyright notice in the Description page of Project Settings.


#include "PawnCombatComponent.h"
#include "ProjectWarrior/Items/WeaponBase.h"
#include "Components/BoxComponent.h"

void UPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, AWeaponBase* InWeaponToRegister, bool bRegisterAsEquippedWeapon)
{
    checkf(!CharacterCarriedWeaponMap.Contains(InWeaponTagToRegister), TEXT("A named named %s has already been added as carried weapon"), *InWeaponTagToRegister.ToString());
    check(InWeaponToRegister);

    CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

    InWeaponToRegister->OnWeaponHitTarget.BindUObject(this, &ThisClass::OnHitTargetActor);
    InWeaponToRegister->OnWeaponPulledFromTarget.BindUObject(this, &ThisClass::OnWeaponPulledFromTargetActor);

    if (bRegisterAsEquippedWeapon)
    {
        CurrentEquippedWeaponTag = InWeaponTagToRegister;
    }

    const FString WeaponString = FString::Printf(TEXT("A weapon named: %s has been registered using the tag %s"), *InWeaponToRegister->GetName(), *InWeaponTagToRegister.ToString());
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, WeaponString);
}

AWeaponBase* UPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const
{
    if (CharacterCarriedWeaponMap.Contains(InWeaponTagToGet))
    {
        if (AWeaponBase* const* FoundWeapon = CharacterCarriedWeaponMap.Find(InWeaponTagToGet))
        {
            return *FoundWeapon;
        }
    }

    return nullptr;
}

AWeaponBase* UPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const
{
    if (!CurrentEquippedWeaponTag.IsValid())
    {
        return nullptr;
    }

    return GetCharacterCarriedWeaponByTag(CurrentEquippedWeaponTag);
}

void UPawnCombatComponent::ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType)
{
    if (ToggleDamageType == EToggleDamageType::CurrentEquippedWeapon)
    {
        AWeaponBase* WeaponToToggle = GetCharacterCurrentEquippedWeapon();

        check(WeaponToToggle);

        if (bShouldEnable)
        {
            WeaponToToggle->GetWeaponCollision()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Weapon Collsion Enabled")));
        }
        else
        {
            WeaponToToggle->GetWeaponCollision()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Weapon Collsion Disabled")));

            OverlappedActors.Empty();
        }
    }
}

void UPawnCombatComponent::OnHitTargetActor(AActor* HitActor)
{
}

void UPawnCombatComponent::OnWeaponPulledFromTargetActor(AActor* InteractedActor)
{
}
