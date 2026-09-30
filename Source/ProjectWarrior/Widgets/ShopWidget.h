#pragma once

#include "CoreMinimal.h"
#include "WarriorWidgetBase.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "ShopWidget.generated.h"

class AWarriorShopActor;
class UPlayerInventoryComponent;
class UDataAsset_Item;

UCLASS()
class PROJECTWARRIOR_API UShopWidget : public UWarriorWidgetBase
{
	GENERATED_BODY()

public:
	void InitShop(AWarriorShopActor* InShop, UPlayerInventoryComponent* InInventory);

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

private:
	UFUNCTION()
	void HandleGoldChanged(int32 NewGold);

	TWeakObjectPtr<AWarriorShopActor> CachedShop;
	TWeakObjectPtr<UPlayerInventoryComponent> CachedInventory;
};