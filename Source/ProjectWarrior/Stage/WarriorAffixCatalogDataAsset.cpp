// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAffixCatalogDataAsset.h"
#include "ProjectWarrior/Stage/WarriorStageSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WarriorAffixCatalog"

const FWarriorAffixDefinition* UWarriorAffixCatalogDataAsset::FindAffix(FName InAffixId) const
{
	return InAffixId.IsNone() ? nullptr : Affixes.Find(InAffixId);
}

bool UWarriorAffixCatalogDataAsset::IsValidAffixId(const FString& InAffixId)
{
	static const FString Prefix = TEXT("affix.");
	if (InAffixId.Len() > 64 || InAffixId.Len() <= Prefix.Len() || !InAffixId.StartsWith(Prefix, ESearchCase::CaseSensitive))
	{
		return false;
	}

	for (const TCHAR Char : InAffixId)
	{
		const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('.');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

EWarriorAffixTier UWarriorAffixCatalogDataAsset::GetTierForCount(int32 InAffixCount)
{
	if (InAffixCount <= 0)
	{
		return EWarriorAffixTier::None;
	}
	return InAffixCount == 1 ? EWarriorAffixTier::Champion : EWarriorAffixTier::Rare;
}

FLinearColor UWarriorAffixCatalogDataAsset::GetTierColor(EWarriorAffixTier InTier) const
{
	switch (InTier)
	{
	case EWarriorAffixTier::Champion:
		return ChampionColor;
	case EWarriorAffixTier::Rare:
		return RareColor;
	default:
		return FLinearColor::White;
	}
}

const UWarriorAffixCatalogDataAsset* UWarriorAffixCatalogDataAsset::LoadFromSettings()
{
	const UWarriorStageSettings* Settings = GetDefault<UWarriorStageSettings>();
	return Settings ? Settings->AffixCatalog.LoadSynchronous() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UWarriorAffixCatalogDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const TPair<FName, FWarriorAffixDefinition>& Pair : Affixes)
	{
		const FString Key = Pair.Key.ToString();
		if (!IsValidAffixId(Key))
		{
			Context.AddError(FText::Format(LOCTEXT("Invalid_Key", "Affix key '{0}' must start with 'affix.' and use only lowercase letters, digits and dots (max 64)."),
				FText::FromString(Key)));
			Result = EDataValidationResult::Invalid;
		}
		if (!Pair.Value.AffixTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("Invalid_NoTag", "Affix key '{0}' has no AI.Affix tag."), FText::FromString(Key)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
