// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "WarriorShopActor.generated.h"

class UDataAsset_Item;
class UDataAsset_Shop;
class UPlayerInventoryComponent;
class UShopWidget;

UCLASS()
class PROJECTWARRIOR_API AWarriorShopActor : public AActor
{
	GENERATED_BODY()
	
public:
	AWarriorShopActor();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	EWarriorPurchaseResult PurchaseItem(UPlayerInventoryComponent* InInventory, UDataAsset_Item* InItem, int32 InCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void OpenShop(APlayerController* InPlayerController);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void CloseShop();

	UDataAsset_Shop* GetShopData() const { return ShopData; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDataAsset_Shop> ShopData;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UShopWidget> ShopWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UShopWidget> ShopWidget;
};
