#include "ShopWidget.h"
#include "ProjectWarrior/Shop/WarriorShopActor.h"
#include "ProjectWarrior/DataAssets/DataAsset_Shop.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/Components/Upgrade/StageUpgradeComponent.h"

void UShopWidget::InitShop(AWarriorShopActor* InShop, UPlayerInventoryComponent* InInventory, UStageUpgradeComponent* InUpgradeComp)
{
	if (CachedInventory.IsValid())
	{
		CachedInventory->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}

	CachedShop = InShop;
	CachedInventory = InInventory;
	InInventory->OnGoldChanged.AddDynamic(this, &ThisClass::HandleGoldChanged);

	TArray<UDataAsset_Item*> Items;
	if (const UDataAsset_Shop* ShopData = InShop->GetShopData())
	{
		Items.Append(ShopData->Items);
	}
	BP_OnShopOpened(Items, InInventory->GetGold());

	CachedUpgradeComp = InUpgradeComp;
	if (const UDataAsset_Shop* ShopData = InShop->GetShopData())
	{
		TArray<UDataAsset_Upgrade*> Upgrades;
		Upgrades.Append(ShopData->Upgrades);
		BP_OnUpgradesOpened(Upgrades);
	}
}

void UShopWidget::NativeDestruct()
{
	if (CachedInventory.IsValid())
	{
		CachedInventory->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}
	Super::NativeDestruct();
}

EWarriorPurchaseResult UShopWidget::RequestPurchase(UDataAsset_Item* InItem, int32 InCount)
{
	EWarriorPurchaseResult Result = EWarriorPurchaseResult::InvalidItem;
	if (CachedShop.IsValid() && CachedInventory.IsValid())
	{
		Result = CachedShop->PurchaseItem(CachedInventory.Get(), InItem, InCount);
	}
	BP_OnPurchaseResult(Result, InItem);
	return Result;
}

void UShopWidget::RequestClose()
{
	if (CachedShop.IsValid())
	{
		CachedShop->CloseShop();
	}
}

void UShopWidget::HandleGoldChanged(int32 NewGold)
{
	BP_OnGoldChanged(NewGold);
}

EWarriorPurchaseResult UShopWidget::RequestUpgrade(UDataAsset_Upgrade* InUpgrade)
{
	EWarriorPurchaseResult Result = EWarriorPurchaseResult::InvalidItem;
	if (CachedShop.IsValid() && CachedInventory.IsValid() && CachedUpgradeComp.IsValid())
	{
		Result = CachedShop->PurchaseUpgrade(CachedInventory.Get(), CachedUpgradeComp.Get(), InUpgrade);
	}
	BP_OnUpgradeResult(Result, InUpgrade, GetUpgradeLevel(InUpgrade));
	return Result;
}

int32 UShopWidget::GetUpgradeLevel(const UDataAsset_Upgrade* InUpgrade) const
{
	return CachedUpgradeComp.IsValid() ? CachedUpgradeComp->GetLevel(InUpgrade) : 0;
}