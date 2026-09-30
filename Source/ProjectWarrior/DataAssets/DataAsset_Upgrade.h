#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DataAsset_Upgrade.generated.h"

class UGameplayEffect;

UCLASS()
class PROJECTWARRIOR_API UDataAsset_Upgrade : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 저장/서버 전송용 고유 키 (예: Upgrade.Stage.AttackPower). 정한 뒤에는 바꾸지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Upgrade"))
	FGameplayTag UpgradeId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSoftObjectPtr<UTexture2D> SoftIconTexture;

	// Infinite, SetByCaller(Shared.SetByCaller.Upgrade) Add 모디파이어를 가진 GE
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSubclassOf<UGameplayEffect> UpgradeEffect;

	// 레벨별 "누적" 증가량. 인덱스 0 = 1레벨 (예: 5, 10, 16)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TArray<float> ValuePerLevel;

	// 해당 레벨로 올리는 비용. 인덱스 0 = 0→1레벨 비용 (예: 100, 200, 400)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TArray<int32> CostPerLevel;

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetMaxLevel() const { return FMath::Min(ValuePerLevel.Num(), CostPerLevel.Num()); }

	// 현재 레벨에서 다음 레벨로 가는 비용. 최대 레벨이면 -1
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetNextCost(int32 InCurrentLevel) const
	{
		return CostPerLevel.IsValidIndex(InCurrentLevel) && InCurrentLevel < GetMaxLevel() ? CostPerLevel[InCurrentLevel] : -1;
	}

	float GetValueAtLevel(int32 InLevel) const
	{
		return ValuePerLevel.IsValidIndex(InLevel - 1) ? ValuePerLevel[InLevel - 1] : 0.f;
	}
};