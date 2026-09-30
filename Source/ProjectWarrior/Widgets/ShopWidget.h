#pragma once

#include "CoreMinimal.h"
#include "WarriorWidgetBase.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "ShopWidget.generated.h"

class AWarriorShopActor;
class UPlayerInventoryComponent;
class UDataAsset_Item;
class UStageUpgradeComponent;
class UDataAsset_Upgrade;

UCLASS()
class PROJECTWARRIOR_API UShopWidget : public UWarriorWidgetBase
{
	GENERATED_BODY()

public:
	void InitShop(AWarriorShopActor* InShop, UPlayerInventoryComponent* InInventory, UStageUpgradeComponent* InUpgradeComp);

protected:
	virtual void NativeDestruct() override;

	// 구매 버튼에서 호출
	UFUNCTION(BlueprintCallable, Category = "Shop")
	EWarriorPurchaseResult RequestPurchase(UDataAsset_Item* InItem, int32 InCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void RequestClose();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Shop Opened"))
	void BP_OnShopOpened(const TArray<UDataAsset_Item*>& InItems, int32 InGold);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Gold Changed"))
	void BP_OnGoldChanged(int32 NewGold);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Purchase Result"))
	void BP_OnPurchaseResult(EWarriorPurchaseResult Result, UDataAsset_Item* InItem);

protected:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	EWarriorPurchaseResult RequestUpgrade(UDataAsset_Upgrade* InUpgrade);

	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetUpgradeLevel(const UDataAsset_Upgrade* InUpgrade) const;

	// 기존 BP_OnShopOpened는 건드리지 않고, 강화 목록용 이벤트를 따로 추가
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Upgrades Opened"))
	void BP_OnUpgradesOpened(const TArray<UDataAsset_Upgrade*>& InUpgrades);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Upgrade Result"))
	void BP_OnUpgradeResult(EWarriorPurchaseResult Result, UDataAsset_Upgrade* InUpgrade, int32 NewLevel);

private:
	UFUNCTION()
	void HandleGoldChanged(int32 NewGold);

	TWeakObjectPtr<AWarriorShopActor> CachedShop;

	TWeakObjectPtr<UPlayerInventoryComponent> CachedInventory;
	
	TWeakObjectPtr<UStageUpgradeComponent> CachedUpgradeComp;
};