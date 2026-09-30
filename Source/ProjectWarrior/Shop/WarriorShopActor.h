// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "ProjectWarrior/Interfaces/WarriorInteractableInterface.h"
#include "WarriorShopActor.generated.h"

class UDataAsset_Item;
class UDataAsset_Shop;
class UPlayerInventoryComponent;
class UShopWidget;
class UStaticMeshComponent;
class UWidgetComponent;

UCLASS()
class PROJECTWARRIOR_API AWarriorShopActor : public AActor, public IWarriorInteractableInterface
{
	GENERATED_BODY()
	
public:
	AWarriorShopActor();

	virtual bool CanInteract(APawn* InInteractor) const override;
	virtual void Interact(APawn* InInteractor) override;
	virtual void SetInteractionFocus(bool bInFocoused) override;

	bool IsShopAvailable() const;

	bool IsShopOpen() const;

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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UStaticMeshComponent> ShopMesh;

	// "E 상점 열기" 프롬프트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UWidgetComponent> PromptWidget;

private:
	UPROPERTY()
	TObjectPtr<UShopWidget> ShopWidget;
};
