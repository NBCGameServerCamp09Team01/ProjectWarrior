// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DataAsset_Item.generated.h"


class UGameplayEffect;

UCLASS()
class PROJECTWARRIOR_API UDataAsset_Item : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> SoftIconTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Price = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 99;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	bool bConsumable = true;

	// 사용 시 자신에게 적용할 효과 (예: GE_Heal)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	TSubclassOf<UGameplayEffect> UseEffect;

	// UseEffect에 값을 넘길 SetByCaller 태그 (예: Shared.SetByCaller.Heal)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use", meta = (Categories = "Shared.SetByCaller"))
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	float EffectMagnitude = 0.f;
	
};
