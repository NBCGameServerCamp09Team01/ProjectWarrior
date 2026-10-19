// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorWidgetBase.h"
#include "ProjectWarrior/Types/WarriorStructTypes.h"
#include "InventoryWheelWidget.generated.h"

class UPlayerInventoryComponent;
class UDataAsset_Item;


UCLASS()
class PROJECTWARRIOR_API UInventoryWheelWidget : public UWarriorWidgetBase
{
	GENERATED_BODY()

public:
	static constexpr int32 WheelSlotCount = 8;

	void OpenWheel(UPlayerInventoryComponent* InInventory);

	// 휠을 닫고 선택된 아이템을 반환 (선택 없음 또는 빈 칸이면 nullptr)
	UDataAsset_Item* CloseWheel();

	bool IsWheelOpen() const { return bIsOpen; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 8칸 내용이 바뀌었을 때 (빈 칸은 ItemData == nullptr)
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Wheel Slots Updated"))
	void BP_OnWheelSlotsUpdated(const TArray<FWarriorInventorySlot>& InWheelSlots);

	// 선택 칸이 바뀌었을 때 (-1 = 선택 없음)
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Selected Index Changed"))
	void BP_OnSelectedIndexChanged(int32 NewIndex, UDataAsset_Item* SelectedItem);

	// 중앙에서 이 거리(px) 안이면 선택 없음
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	float DeadZoneRadius = 60.f;

	// 휠이 열려 있는 동안 게임 속도 감소 ==> 고민중
	UPROPERTY(EditDefaultsOnly, Category = "Wheel")
	bool bSlowTimeWhileOpen = false;

	UPROPERTY(EditDefaultsOnly, Category = "Wheel", meta = (EditCondition = "bSlowTimeWhileOpen", ClampMin = "0.05", ClampMax = "1.0"))
	float OpenTimeDilation = 0.3f;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	int32 CalcIndexFromMouse() const;

	UDataAsset_Item* GetItemAt(int32 InIndex) const;

	TWeakObjectPtr<UPlayerInventoryComponent> CachedInventory;

	UPROPERTY()
	TArray<FWarriorInventorySlot> WheelSlots;

	int32 SelectedIndex = INDEX_NONE;

	bool bIsOpen = false;
};