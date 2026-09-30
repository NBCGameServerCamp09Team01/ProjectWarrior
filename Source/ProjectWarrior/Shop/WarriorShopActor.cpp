
#include "WarriorShopActor.h"
#include "ProjectWarrior/DataAssets/DataAsset_Shop.h"
#include "ProjectWarrior/DataAssets/DataAsset_Item.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "ProjectWarrior/Widgets/ShopWidget.h"

AWarriorShopActor::AWarriorShopActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

EWarriorPurchaseResult AWarriorShopActor::PurchaseItem(UPlayerInventoryComponent* InInventory, UDataAsset_Item* InItem, int32 InCount)
{
	// 이 상점에서 파는 아이템인지 확인 (UI 조작으로 다른 아이템을 사는 것 방지)
	if (!InInventory || !InItem || InCount <= 0 || !ShopData || !ShopData->Items.Contains(InItem))
	{
		return EWarriorPurchaseResult::InvalidItem;
	}

	// 골드보다 인벤토리 공간을 먼저 확인해야 골드만 빠지는 일이 없다
	if (!InInventory->CanAddItem(InItem, InCount))
	{
		return EWarriorPurchaseResult::InventoryFull;
	}

	const int64 TotalPrice = static_cast<int64>(InItem->Price) * InCount;
	if (TotalPrice > InInventory->GetGold())
	{
		return EWarriorPurchaseResult::NotEnoughGold;
	}

	InInventory->SpendGold(static_cast<int32>(TotalPrice));
	InInventory->AddItem(InItem, InCount);
	return EWarriorPurchaseResult::Success;
}

void AWarriorShopActor::OpenShop(APlayerController* InPlayerController)
{
	if (!InPlayerController || !ShopWidgetClass)
	{
		return;
	}

	AWarriorPlayerState* PlayerState = InPlayerController->GetPlayerState<AWarriorPlayerState>();
	UPlayerInventoryComponent* Inventory = PlayerState ? PlayerState->GetPlayerInventoryComponent() : nullptr;
	if (!Inventory)
	{
		return;
	}

	if (!ShopWidget)
	{
		ShopWidget = CreateWidget<UShopWidget>(InPlayerController, ShopWidgetClass);
	}
	ShopWidget->InitShop(this, Inventory);
	ShopWidget->AddToViewport(20);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ShopWidget->TakeWidget());
	InPlayerController->SetInputMode(InputMode);
	InPlayerController->SetShowMouseCursor(true);
}

void AWarriorShopActor::CloseShop()
{
	if (!ShopWidget)
	{
		return;
	}

	if (APlayerController* PC = ShopWidget->GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	ShopWidget->RemoveFromParent();
}

