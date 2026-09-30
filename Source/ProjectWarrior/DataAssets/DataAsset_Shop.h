#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataAsset_Shop.generated.h"

class UDataAsset_Item;
class UDataAsset_Upgrade;

UCLASS()
class PROJECTWARRIOR_API UDataAsset_Shop : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	FText ShopName;

	// 판매 목록. 가격은 각 아이템의 Price를 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDataAsset_Item>> Items;

	// 능력치 강화 목록. 비용은 각 강화의 CostPerLevel을 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDataAsset_Upgrade>> Upgrades;
};