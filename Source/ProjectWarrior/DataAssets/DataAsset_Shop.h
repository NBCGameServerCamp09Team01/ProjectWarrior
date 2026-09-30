#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataAsset_Shop.generated.h"

class UDataAsset_Item;

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
};