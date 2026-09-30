// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerInventoryComponent.h"
#include "GameFramework/PlayerState.h"
#include "ProjectWarrior/DataAssets/DataAsset_Item.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"

UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPlayerInventoryComponent::AddItem(UDataAsset_Item* InItem, int32 InCount)
{
	if (!InItem || InCount <= 0)
	{
		return false;
	}

	if (FWarriorInventorySlot* Slot = FindSlot(InItem))
	{
		if (Slot->Quantity + InCount > InItem->MaxStack)
		{
			return false;
		}
		Slot->Quantity += InCount;
	}
	else
	{
		if (InCount > InItem->MaxStack)
		{
			return false;
		}
		FWarriorInventorySlot& NewSlot = Slots.AddDefaulted_GetRef();
		NewSlot.ItemData = InItem;
		NewSlot.Quantity = InCount;
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UPlayerInventoryComponent::CanAddItem(const UDataAsset_Item* InItem, int32 InCount) const
{
	if (!InItem || InCount <= 0)
	{
		return false;
	}
	return GetItemCount(InItem) + InCount <= InItem->MaxStack;
}

bool UPlayerInventoryComponent::RemoveItem(UDataAsset_Item* InItem, int32 InCount)
{
	if (!InItem || InCount <= 0)
	{
		return false;
	}

	const int32 Index = Slots.IndexOfByPredicate(
		[InItem](const FWarriorInventorySlot& Slot) { return Slot.ItemData == InItem; });

	if (Index == INDEX_NONE || Slots[Index].Quantity < InCount)
	{
		return false;
	}

	Slots[Index].Quantity -= InCount;
	if (Slots[Index].IsEmpty())
	{
		Slots.RemoveAt(Index);
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UPlayerInventoryComponent::UseItem(UDataAsset_Item* InItem)
{
	if (!InItem || !InItem->UseEffect || GetItemCount(InItem) <= 0)
	{
		return false;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return false;
	}

	// 사망 상태에서는 사용 불가
	FGameplayTagContainer DeathTags;
	DeathTags.AddTag(WarriorGameplayTags::Shared_Status_Death_Normal);
	DeathTags.AddTag(WarriorGameplayTags::Shared_Status_Death_Finisher);
	DeathTags.AddTag(WarriorGameplayTags::Shared_Status_Death_Knockback);

	if (UWarriorFunctionLibrary::NativeDoesActorHaveAnyTag(OwningPawn, DeathTags))
	{
		return false;
	}

	UWarriorAbilitySystemComponent* ASC = UWarriorFunctionLibrary::NativeGetWarriorASCFromActor(OwningPawn);

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(InItem);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InItem->UseEffect, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	if (InItem->SetByCallerTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(InItem->SetByCallerTag, InItem->EffectMagnitude);
	}

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	if (InItem->bConsumable)
	{
		RemoveItem(InItem, 1);
	}
	return true;
}

int32 UPlayerInventoryComponent::GetItemCount(const UDataAsset_Item* InItem) const
{
	const FWarriorInventorySlot* Slot = Slots.FindByPredicate(
		[InItem](const FWarriorInventorySlot& Slot) { return Slot.ItemData == InItem; });

	return Slot ? Slot->Quantity : 0;
}

void UPlayerInventoryComponent::AddGold(int32 InAmount)
{
	if (InAmount <= 0)
	{
		return;
	}
	Gold += InAmount;
	OnGoldChanged.Broadcast(Gold);
}

bool UPlayerInventoryComponent::SpendGold(int32 InAmount)
{
	if (InAmount < 0 || Gold < InAmount)
	{
		return false;
	}
	Gold -= InAmount;
	OnGoldChanged.Broadcast(Gold);
	return true;
}

FWarriorInventorySlot* UPlayerInventoryComponent::FindSlot(const UDataAsset_Item* InItem)
{
	return Slots.FindByPredicate(
		[InItem](const FWarriorInventorySlot& Slot) { return Slot.ItemData == InItem; });
}

APawn* UPlayerInventoryComponent::GetOwningPlayerPawn() const
{
	const APlayerState* OwningPlayerState = GetOwner<APlayerState>();
	return OwningPlayerState ? OwningPlayerState->GetPawn() : nullptr;
}