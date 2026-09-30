// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/Interfaces/PawnCombatInterface.h"
#include "ProjectWarrior/Interfaces/PawnUIInterface.h"
#include "ProjectWarrior/Components/UI/PawnUIComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

UWarriorAttributeSet::UWarriorAttributeSet()
{
    InitCurrentHealth(1.f);
    InitMaxHealth(1.f);
    InitCurrentStamina(1.f);
    InitMaxStamina(1.f);
    InitAttackPower(1.f);
    InitDefensePower(1.f);

	InitMaxBalance(1.f);
	InitCurrentBalance(1.f);
}

void UWarriorAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if (!CachedPawnUIInterface.IsValid())
	{
		CachedPawnUIInterface = TWeakInterfacePtr<IPawnUIInterface>(Data.Target.GetAvatarActor());
	}

	checkf(CachedPawnUIInterface.IsValid(), TEXT("%s didn't implement IPawnUIInterface"), *Data.Target.GetAvatarActor()->GetActorNameOrLabel());

	UPawnUIComponent* PawnUIComponent = CachedPawnUIInterface->GetPawnUIComponent();

	checkf(PawnUIComponent, TEXT("Couldn't extract a PawnUIComponent from %s"), *Data.Target.GetAvatarActor()->GetActorNameOrLabel());

	if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		const float NewCurrentHealth = FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		PawnUIComponent->OnCurrentHealthChanged.Broadcast(GetCurrentHealth() / GetMaxHealth());
	}

	if (Data.EvaluatedData.Attribute == GetCurrentStaminaAttribute())
	{
		const float NewCurrentStamina = FMath::Clamp(GetCurrentStamina(), 0.f, GetMaxStamina());

		SetCurrentStamina(NewCurrentStamina);

		PawnUIComponent->OnCurrentStaminaChanged.Broadcast(GetCurrentStamina() / GetMaxStamina());
	}

	if (Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
	{
		const float OldHealth = GetCurrentHealth();
		const float DamageDone = GetDamageTaken();

		const float NewCurrentHealth = FMath::Clamp(OldHealth - DamageDone, 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		const FString DebugString = FString::Printf(
			TEXT("Old Health: %f, Damage Done: %f, NewCurrentHealth: %f"),
			OldHealth,
			DamageDone,
			NewCurrentHealth
		);

		PawnUIComponent->OnCurrentHealthChanged.Broadcast(GetCurrentHealth() / GetMaxHealth());

		//TODO::Handle character death
		if (GetCurrentHealth() == 0.f)
		{
			FGameplayTag CheckKnockBack = FGameplayTag::RequestGameplayTag(FName("Shared.Event.HitReact.KnockBack"));

			if (Data.EffectSpec.GetDynamicAssetTags().HasTag(CheckKnockBack))
			{
				UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(), WarriorGameplayTags::Shared_Status_Death_Knockback);
			}
			else
			{
				UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(), WarriorGameplayTags::Shared_Status_Death_Normal);
			}
		}
	}

	if (Data.EvaluatedData.Attribute == GetBalanceDamageTakenAttribute())
	{
		const float OldBalance = GetCurrentBalance();
		const float BalanceDamageDone = GetBalanceDamageTaken();

		const float NewCurrentBalance = FMath::Clamp(OldBalance - BalanceDamageDone, 0.f, GetMaxBalance());

		SetCurrentBalance(NewCurrentBalance);

		if (GetCurrentBalance() == 0.f)
		{
			FGameplayEventData EventData;

			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor
			(
				Data.Target.GetAvatarActor(),
				WarriorGameplayTags::Shared_Event_Stagger,
				EventData
			);
		}
	}
}



void UWarriorAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// 최대치가 줄어든 경우(강화 리셋 등) 현재값이 넘치지 않게.
	// SetCurrent* 호출이 PostAttributeChange(Current*)를 다시 부르므로 UI도 거기서 갱신된다.
	if (Attribute == GetMaxHealthAttribute() && GetCurrentHealth() > NewValue)
	{
		SetCurrentHealth(NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetCurrentStamina() > NewValue)
	{
		SetCurrentStamina(NewValue);
	}

	UPawnUIComponent* PawnUIComponent = FindPawnUIComponent();
	if (!PawnUIComponent)
	{
		return;  // 초기화 중(아바타/UI 컴포넌트 준비 전)에는 건너뛴다
	}

	if ((Attribute == GetCurrentHealthAttribute() || Attribute == GetMaxHealthAttribute()) && GetMaxHealth() > 0.f)
	{
		PawnUIComponent->OnCurrentHealthChanged.Broadcast(GetCurrentHealth() / GetMaxHealth());
	}
	else if ((Attribute == GetCurrentStaminaAttribute() || Attribute == GetMaxStaminaAttribute()) && GetMaxStamina() > 0.f)
	{
		PawnUIComponent->OnCurrentStaminaChanged.Broadcast(GetCurrentStamina() / GetMaxStamina());
	}
}

UPawnUIComponent* UWarriorAttributeSet::FindPawnUIComponent() const
{
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const IPawnUIInterface* UIInterface = ASC ? Cast<IPawnUIInterface>(ASC->GetAvatarActor()) : nullptr;
	return UIInterface ? UIInterface->GetPawnUIComponent() : nullptr;
}