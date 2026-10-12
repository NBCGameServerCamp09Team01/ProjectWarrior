#include "WarriorStageCatalogDataAsset.h"
#include "WarriorStageWaveDataAsset.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WarriorStage"

const FWarriorStageCatalogEntry* UWarriorStageCatalogDataAsset::FindEntry(FName InStageId) const
{
	if (InStageId.IsNone())
	{
		return nullptr;
	}
	return Stages.FindByPredicate([InStageId](const FWarriorStageCatalogEntry& Entry)
	{
		return Entry.StageId == InStageId;
	});
}

const FWarriorStageCatalogEntry* UWarriorStageCatalogDataAsset::GetFirstEntry() const
{
	return Stages.Num() > 0 ? &Stages[0] : nullptr;
}

const FWarriorStageCatalogEntry* UWarriorStageCatalogDataAsset::FindFirstEntryByLevel(const FString& InLevelPackageName) const
{
	if (InLevelPackageName.IsEmpty())
	{
		return nullptr;
	}
	return Stages.FindByPredicate([&InLevelPackageName](const FWarriorStageCatalogEntry& Entry)
	{
		return !Entry.Level.IsNull() && Entry.Level.ToSoftObjectPath().GetLongPackageName() == InLevelPackageName;
	});
}

bool UWarriorStageCatalogDataAsset::IsValidStageKey(FName InKey)
{
	if (InKey.IsNone())
	{
		return false;
	}

	const FString KeyText = InKey.ToString();
	if (KeyText.Len() > 64)
	{
		return false;
	}
	for (const TCHAR Char : KeyText)
	{
		const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('.');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UWarriorStageCatalogDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&](const FText& InMessage)
	{
		Context.AddError(InMessage);
		Result = EDataValidationResult::Invalid;
	};

	TSet<FName> SeenStageIds;
	for (int32 Index = 0; Index < Stages.Num(); ++Index)
	{
		const FWarriorStageCatalogEntry& Entry = Stages[Index];

		if (!IsValidStageKey(Entry.StageId))
		{
			AddError(FText::Format(LOCTEXT("Invalid_StageKey", "Stages[{0}] StageId '{1}' must be 1-64 chars of lowercase letters, digits or '.' (same as the server stageId)."),
				Index, FText::FromName(Entry.StageId)));
		}
		else if (SeenStageIds.Contains(Entry.StageId))
		{
			AddError(FText::Format(LOCTEXT("Invalid_DuplicateStage", "Stage {0} is listed more than once."), FText::FromName(Entry.StageId)));
		}
		SeenStageIds.Add(Entry.StageId);

		if (!Entry.RegionId.IsNone() && !IsValidStageKey(Entry.RegionId))
		{
			AddError(FText::Format(LOCTEXT("Invalid_RegionKey", "Stages[{0}] RegionId '{1}' must be lowercase letters, digits or '.'."),
				Index, FText::FromName(Entry.RegionId)));
		}

		if (Entry.Level.IsNull())
		{
			AddError(FText::Format(LOCTEXT("Invalid_NoLevel", "Stage {0} has no Level."), FText::FromName(Entry.StageId)));
		}

		if (Entry.WaveData.IsNull())
		{
			Context.AddWarning(FText::Format(LOCTEXT("Warning_NoWaveData", "Stage {0} has no WaveData. The level's FlowManager waves will be used."), FText::FromName(Entry.StageId)));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
