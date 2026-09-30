#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "DataAsset_Upgrade.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EWarriorUpgradeCostGrowth : uint8
{
	Linear,       // BaseCost + CostStep * Level
	Exponential   // BaseCost * CostMultiplier ^ Level
};

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

	// 이 강화로 늘어나는 최대치 어트리뷰트 (예: MaxHealth). 비워두면 회복 처리 안 함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Restore")
	FGameplayAttribute IncreasedAttribute;

	// 최대치가 늘어난 만큼 채워줄 현재값 어트리뷰트 (예: CurrentHealth)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Restore")
	FGameplayAttribute RestoreAttribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Level", meta = (ClampMin = "1"))
	int32 MaxLevel = 5;

	// 레벨 1당 증가량. 레벨 N의 총 증가량 = ValuePerLevel * N
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Level")
	float ValuePerLevel = 10.f;

	// 0 → 1레벨 비용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Cost", meta = (ClampMin = "0"))
	int32 BaseCost = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Cost")
	EWarriorUpgradeCostGrowth CostGrowth = EWarriorUpgradeCostGrowth::Linear;

	// Linear: 레벨마다 더해지는 비용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Cost", meta = (ClampMin = "0", EditCondition = "CostGrowth == EWarriorUpgradeCostGrowth::Linear", EditConditionHides))
	int32 CostStep = 50;

	// Exponential: 레벨마다 곱해지는 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Cost", meta = (ClampMin = "1.0", EditCondition = "CostGrowth == EWarriorUpgradeCostGrowth::Exponential", EditConditionHides))
	float CostMultiplier = 1.5f;

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetMaxLevel() const { return MaxLevel; }

	// 현재 레벨에서 다음 레벨로 가는 비용. 최대 레벨이면 -1
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetNextCost(int32 InCurrentLevel) const;

	// 레벨 N의 누적 증가량
	float GetValueAtLevel(int32 InLevel) const;
};
