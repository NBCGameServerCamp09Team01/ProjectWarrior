#include "WarriorStatPanelWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"

namespace
{
	//공격력은 포인트당 0.5씩 오르므로 소수 한 자리까지 보여 준다
	FText ToStatText(float InValue)
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 1;
		return FText::AsNumber(InValue, &Options);
	}

	FText ToCurrentMaxText(float InCurrent, float InMax)
	{
		return FText::Format(INVTEXT("{0} / {1}"), FText::AsNumber(FMath::RoundToInt(InCurrent)), FText::AsNumber(FMath::RoundToInt(InMax)));
	}
}

void UWarriorStatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwningPlayerPawn());
	if (!CachedASC.IsValid())
	{
		return;
	}

	BindAttribute(UWarriorAttributeSet::GetCurrentHealthAttribute());
	BindAttribute(UWarriorAttributeSet::GetMaxHealthAttribute());
	BindAttribute(UWarriorAttributeSet::GetCurrentStaminaAttribute());
	BindAttribute(UWarriorAttributeSet::GetMaxStaminaAttribute());
	BindAttribute(UWarriorAttributeSet::GetAttackPowerAttribute());
	BindAttribute(UWarriorAttributeSet::GetDefensePowerAttribute());

	RefreshAll();
}

void UWarriorStatPanelWidget::NativeDestruct()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : BoundHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}
	BoundHandles.Reset();
	CachedASC.Reset();

	Super::NativeDestruct();
}

void UWarriorStatPanelWidget::BindAttribute(const FGameplayAttribute& InAttribute)
{
	const FDelegateHandle Handle = CachedASC->GetGameplayAttributeValueChangeDelegate(InAttribute).AddUObject(this, &ThisClass::HandleAttributeChanged);
	BoundHandles.Emplace(InAttribute, Handle);
}

void UWarriorStatPanelWidget::HandleAttributeChanged(const FOnAttributeChangeData& InData)
{
	RefreshAll();

	//현재값(피격·소모)은 연출하지 않는다
	if (InData.Attribute == UWarriorAttributeSet::GetMaxHealthAttribute())
	{
		BP_OnStatUpgraded(EWarriorPanelStat::Health, InData.NewValue, InData.OldValue);
	}
	else if (InData.Attribute == UWarriorAttributeSet::GetMaxStaminaAttribute())
	{
		BP_OnStatUpgraded(EWarriorPanelStat::Stamina, InData.NewValue, InData.OldValue);
	}
	else if (InData.Attribute == UWarriorAttributeSet::GetAttackPowerAttribute())
	{
		BP_OnStatUpgraded(EWarriorPanelStat::AttackPower, InData.NewValue, InData.OldValue);
	}
	else if (InData.Attribute == UWarriorAttributeSet::GetDefensePowerAttribute())
	{
		BP_OnStatUpgraded(EWarriorPanelStat::DefensePower, InData.NewValue, InData.OldValue);
	}
}

void UWarriorStatPanelWidget::RefreshAll()
{
	const UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return;
	}

	auto Get = [ASC](const FGameplayAttribute& InAttribute) { return ASC->GetNumericAttribute(InAttribute); };

	if (Text_HealthValue)
	{
		Text_HealthValue->SetText(ToCurrentMaxText(Get(UWarriorAttributeSet::GetCurrentHealthAttribute()), Get(UWarriorAttributeSet::GetMaxHealthAttribute())));
	}
	if (Text_StaminaValue)
	{
		Text_StaminaValue->SetText(ToCurrentMaxText(Get(UWarriorAttributeSet::GetCurrentStaminaAttribute()), Get(UWarriorAttributeSet::GetMaxStaminaAttribute())));
	}
	if (Text_AttackValue)
	{
		Text_AttackValue->SetText(ToStatText(Get(UWarriorAttributeSet::GetAttackPowerAttribute())));
	}
	if (Text_DefenseValue)
	{
		Text_DefenseValue->SetText(ToStatText(Get(UWarriorAttributeSet::GetDefensePowerAttribute())));
	}
}