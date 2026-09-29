// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryWheelWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/DataAssets/DataAsset_Item.h"

void UInventoryWheelWidget::OpenWheel(UPlayerInventoryComponent* InInventory)
{
	if (!InInventory || bIsOpen)
	{
		return;
	}

	bIsOpen = true;

	CachedInventory = InInventory;
	CachedInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	HandleInventoryChanged();

	SelectedIndex = INDEX_NONE;
	BP_OnSelectedIndexChanged(SelectedIndex, nullptr);

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (APlayerController* PC = GetOwningPlayer())
	{
		// 커서를 화면 중앙에 두고, 카메라 회전을 막는다
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PC->GetViewportSize(ViewportX, ViewportY);
		PC->SetMouseLocation(ViewportX / 2, ViewportY / 2);
		PC->SetShowMouseCursor(true);
		PC->SetIgnoreLookInput(true);

		// 게임 입력도 계속 받아야 I 키를 뗀 것(Completed)을 감지할 수 있다
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PC->SetInputMode(InputMode);
	}

	if (bSlowTimeWhileOpen)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, OpenTimeDilation);
	}
}

UDataAsset_Item* UInventoryWheelWidget::CloseWheel()
{
	if (!bIsOpen)
	{
		return nullptr;
	}

	bIsOpen = false;

	UDataAsset_Item* SelectedItem = GetItemAt(SelectedIndex);

	if (CachedInventory.IsValid())
	{
		CachedInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->ResetIgnoreLookInput();
		PC->SetInputMode(FInputModeGameOnly());
	}

	if (bSlowTimeWhileOpen)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}

	return SelectedItem;
}

void UInventoryWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsOpen)
	{
		return;
	}

	const int32 NewIndex = CalcIndexFromMouse();
	if (NewIndex != SelectedIndex)
	{
		SelectedIndex = NewIndex;
		BP_OnSelectedIndexChanged(SelectedIndex, GetItemAt(SelectedIndex));
	}
}

void UInventoryWheelWidget::HandleInventoryChanged()
{
	// 인벤토리 앞에서부터 8칸을 휠에 배치하고, 모자란 칸은 빈 칸으로 채운다
	WheelSlots.Init(FWarriorInventorySlot(), WheelSlotCount);

	if (CachedInventory.IsValid())
	{
		const TArray<FWarriorInventorySlot> InventorySlots = CachedInventory->GetSlots();
		const int32 Count = FMath::Min(InventorySlots.Num(), WheelSlotCount);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			WheelSlots[Index] = InventorySlots[Index];
		}
	}

	BP_OnWheelSlotsUpdated(WheelSlots);
}

int32 UInventoryWheelWidget::CalcIndexFromMouse() const
{
	// 둘 다 DPI 배율이 적용된 위젯 좌표계
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
	const FVector2D ViewportCenter = UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this) * 0.5f;
	const FVector2D Direction = MousePosition - ViewportCenter;

	if (Direction.Size() < DeadZoneRadius)
	{
		return INDEX_NONE;
	}

	// 위쪽이 0도, 시계 방향으로 증가 (화면 좌표는 Y가 아래로 증가)
	float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(Direction.X, -Direction.Y));
	if (AngleDegrees < 0.f)
	{
		AngleDegrees += 360.f;
	}

	// 0번 칸이 위쪽 중앙에 오도록 반 칸만큼 밀어서 계산
	const float SliceAngle = 360.f / WheelSlotCount;
	return FMath::FloorToInt((AngleDegrees + SliceAngle * 0.5f) / SliceAngle) % WheelSlotCount;
}

UDataAsset_Item* UInventoryWheelWidget::GetItemAt(int32 InIndex) const
{
	return WheelSlots.IsValidIndex(InIndex) ? WheelSlots[InIndex].ItemData.Get() : nullptr;
}