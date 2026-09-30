#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "StageUpgradeComponent.generated.h"

class UDataAsset_Upgrade;
class UWarriorAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageUpgradeChanged, FGameplayTag, UpgradeId, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStageUpgradesReset);

/**
 * 스테이지 안에서만 유효한 능력치 강화. AWarriorPlayerState에 부착.
 * 레벨만 소유하고, 실제 수치는 폰의 ASC에 Infinite GE로 적용한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTWARRIOR_API UStageUpgradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStageUpgradeComponent();

	// 비용 차감은 호출하는 쪽(상점)이 한다. 여기서는 레벨업과 GE 적용만
	bool IncreaseLevel(const UDataAsset_Upgrade* InUpgrade);

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsMaxLevel(const UDataAsset_Upgrade* InUpgrade) const;

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	int32 GetLevel(const UDataAsset_Upgrade* InUpgrade) const;

	// 스테이지 종료 시: GE 전부 제거 + 레벨 초기화
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void ResetAll();

	UPROPERTY(BlueprintAssignable)
	FOnStageUpgradeChanged OnUpgradeChanged;

	UPROPERTY(BlueprintAssignable)
	FOnStageUpgradesReset OnUpgradesReset;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 폰이 바뀌면(리스폰 등) 새 ASC에 현재 레벨 전부 재적용
	UFUNCTION()
	void HandlePawnSet(APlayerState* InPlayer, APawn* InNewPawn, APawn* InOldPawn);

	void ApplyUpgrade(const UDataAsset_Upgrade* InUpgrade);
	void RemoveUpgradeEffect(FGameplayTag InUpgradeId);

	UWarriorAbilitySystemComponent* GetOwningASC() const;

	// 키: UpgradeId. 데이터 에셋은 재적용할 때 필요하므로 같이 보관
	UPROPERTY()
	TMap<FGameplayTag, int32> Levels;

	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<const UDataAsset_Upgrade>> UpgradeAssets;

	TMap<FGameplayTag, FActiveGameplayEffectHandle> ActiveHandles;
};