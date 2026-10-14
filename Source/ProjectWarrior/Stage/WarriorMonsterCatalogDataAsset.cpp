// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorMonsterCatalogDataAsset.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Stage/WarriorStageSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WarriorMonsterCatalog"

TSoftClassPtr<AWarriorAICharacter> UWarriorMonsterCatalogDataAsset::FindMonster(FName InMonsterId) const
{
	const TSoftClassPtr<AWarriorAICharacter>* Found = Monsters.Find(InMonsterId);
	return Found ? *Found : TSoftClassPtr<AWarriorAICharacter>();
}

bool UWarriorMonsterCatalogDataAsset::HasMonster(FName InMonsterId) const
{
	return !InMonsterId.IsNone() && !FindMonster(InMonsterId).IsNull();
}

bool UWarriorMonsterCatalogDataAsset::IsValidMonsterId(const FString& InMonsterId)
{
	static const FString Prefix = TEXT("monster.");
	if (InMonsterId.Len() > 64 || InMonsterId.Len() <= Prefix.Len() || !InMonsterId.StartsWith(Prefix, ESearchCase::CaseSensitive))
	{
		return false;
	}

	for (const TCHAR Char : InMonsterId)
	{
		const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('.');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

const UWarriorMonsterCatalogDataAsset* UWarriorMonsterCatalogDataAsset::LoadFromSettings()
{
	const UWarriorStageSettings* Settings = GetDefault<UWarriorStageSettings>();
	return Settings ? Settings->MonsterCatalog.LoadSynchronous() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UWarriorMonsterCatalogDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const TPair<FName, TSoftClassPtr<AWarriorAICharacter>>& Pair : Monsters)
	{
		const FString Key = Pair.Key.ToString();
		if (!IsValidMonsterId(Key))
		{
			Context.AddError(FText::Format(LOCTEXT("Invalid_Key", "Monster key '{0}' must start with 'monster.' and use only lowercase letters, digits and dots (max 64)."),
				FText::FromString(Key)));
			Result = EDataValidationResult::Invalid;
		}
		if (Pair.Value.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("Invalid_NoClass", "Monster key '{0}' has no blueprint."), FText::FromString(Key)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
