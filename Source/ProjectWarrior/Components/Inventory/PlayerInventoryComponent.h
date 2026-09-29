// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectWarrior/Types/WarriorStructTypes.h"
#include "PlayerInventoryComponent.generated.h"

class UDataAsset_Item;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChangedDelegate, int32, NewGold);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTWARRIOR_API UPlayerInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInventoryComponent();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(UDataAsset_Item* InItem, int32 InCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(UDataAsset_Item* InItem, int32 InCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(UDataAsset_Item* InItem);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UDataAsset_Item* InItem) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FWarriorInventorySlot> GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddGold(int32 InAmount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SpendGold(int32 InAmount);

	UPROPERTY(BlueprintAssignable)
	FOnInventoryChangedDelegate OnInventoryChanged;

	UPROPERTY(BlueprintAssignable)
	FOnGoldChangedDelegate OnGoldChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FWarriorInventorySlot> Slots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Gold = 0;

private:
	FWarriorInventorySlot* FindSlot(const UDataAsset_Item* InItem);

	APawn* GetOwningPlayerPawn() const;
};