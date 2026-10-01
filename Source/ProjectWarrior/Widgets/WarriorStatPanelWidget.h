#pragma once

#include "CoreMinimal.h"
#include "WarriorWidgetBase.h"
#include "GameplayEffectTypes.h"
#include "WarriorStatPanelWidget.generated.h"

class UAbilitySystemComponent;
class UTextBlock;

UENUM(BlueprintType)
enum class EWarriorPanelStat : uint8
{
	Health,
	Stamina,
	AttackPower,
	DefensePower
};

/**
 * 스테이지 HUD의 능력치 수치 패널. 체력·스태미나·공격력·방어력을 숫자로 보여 준다.
 * 플레이어 폰의 ASC 어트리뷰트 변경을 구독하므로, 계정 성장·상점 강화 GE가 걸리거나 풀려도 바로 갱신된다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStatPanelWidget : public UWarriorWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//최대치·공격력·방어력이 바뀌었을 때(강화 등). 피격·소모로 현재값이 바뀔 때는 오지 않는다. BP에서 강조 연출에 쓴다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|StatPanel")
	void BP_OnStatUpgraded(EWarriorPanelStat Stat, float NewValue, float OldValue);

	//~ Begin 없어도 되는 위젯 (WBP에 같은 이름으로 두면 연결된다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HealthValue;		// "320 / 350"

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StaminaValue;		// "100 / 120"

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AttackValue;		// "27.5"

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DefenseValue;		// "12"
	//~ End 없어도 되는 위젯

private:
	void BindAttribute(const FGameplayAttribute& InAttribute);
	void HandleAttributeChanged(const FOnAttributeChangeData& InData);
	void RefreshAll();

	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> BoundHandles;
};