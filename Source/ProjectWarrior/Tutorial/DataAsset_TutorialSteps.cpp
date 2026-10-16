// Fill out your copyright notice in the Description page of Project Settings.

#include "DataAsset_TutorialSteps.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "WarriorTutorial"

const FWarriorTutorialStep* UDataAsset_TutorialSteps::FindStep(int32 InChapterIndex, int32 InStepIndex) const
{
	if (!Chapters.IsValidIndex(InChapterIndex) || !Chapters[InChapterIndex].Steps.IsValidIndex(InStepIndex))
	{
		return nullptr;
	}
	return &Chapters[InChapterIndex].Steps[InStepIndex];
}

FGameplayTagContainer UDataAsset_TutorialSteps::GetAllUnlockAbilityTags() const
{
	FGameplayTagContainer AllTags;
	for (const FWarriorTutorialChapter& Chapter : Chapters)
	{
		for (const FWarriorTutorialStep& Step : Chapter.Steps)
		{
			AllTags.AppendTags(Step.UnlockAbilityTags);
		}
	}
	return AllTags;
}

FGameplayTagContainer UDataAsset_TutorialSteps::GetUnlockedAbilityTagsUpTo(int32 InChapterIndex, int32 InStepIndex) const
{
	FGameplayTagContainer Unlocked;
	for (int32 ChapterIndex = 0; ChapterIndex < Chapters.Num() && ChapterIndex <= InChapterIndex; ++ChapterIndex)
	{
		const TArray<FWarriorTutorialStep>& Steps = Chapters[ChapterIndex].Steps;
		//앞 장은 모든 단계, 지금 장은 지금 단계까지
		const int32 LastStepIndex = ChapterIndex < InChapterIndex ? Steps.Num() - 1 : FMath::Min(InStepIndex, Steps.Num() - 1);
		for (int32 StepIndex = 0; StepIndex <= LastStepIndex; ++StepIndex)
		{
			Unlocked.AppendTags(Steps[StepIndex].UnlockAbilityTags);
		}
	}
	return Unlocked;
}

#if WITH_EDITOR
EDataValidationResult UDataAsset_TutorialSteps::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (int32 ChapterIndex = 0; ChapterIndex < Chapters.Num(); ++ChapterIndex)
	{
		const FWarriorTutorialChapter& Chapter = Chapters[ChapterIndex];
		if (Chapter.Steps.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyChapter", "{0}장에 단계가 없습니다."), FText::AsNumber(ChapterIndex + 1)));
			Result = EDataValidationResult::Invalid;
		}

		TSet<FName> StepIds;
		for (const FWarriorTutorialStep& Step : Chapter.Steps)
		{
			const FText StepName = FText::Format(LOCTEXT("StepName", "{0}장 {1}"), FText::AsNumber(ChapterIndex + 1), FText::FromName(Step.StepId));

			bool bAlreadyInSet = false;
			StepIds.Add(Step.StepId, &bAlreadyInSet);
			if (Step.StepId.IsNone() || bAlreadyInSet)
			{
				Context.AddError(FText::Format(LOCTEXT("BadStepId", "{0}: StepId가 비었거나 장 안에서 겹칩니다."), StepName));
				Result = EDataValidationResult::Invalid;
			}

			const bool bNeedsTag = Step.Goal == EWarriorTutorialGoal::AbilityActivated
				|| Step.Goal == EWarriorTutorialGoal::GameplayEvent
				|| Step.Goal == EWarriorTutorialGoal::TagAdded;
			if (bNeedsTag && !Step.GoalTag.IsValid())
			{
				Context.AddError(FText::Format(LOCTEXT("NoGoalTag", "{0}: 완료 신호에 GoalTag가 필요합니다."), StepName));
				Result = EDataValidationResult::Invalid;
			}
			if (Step.Goal == EWarriorTutorialGoal::InputUsed && !Step.KeyAction)
			{
				Context.AddError(FText::Format(LOCTEXT("NoKeyAction", "{0}: InputUsed에는 KeyAction이 필요합니다."), StepName));
				Result = EDataValidationResult::Invalid;
			}
			if (Step.Goal == EWarriorTutorialGoal::ReachZone && Step.ZoneId.IsNone())
			{
				Context.AddError(FText::Format(LOCTEXT("NoZone", "{0}: ReachZone에는 ZoneId가 필요합니다."), StepName));
				Result = EDataValidationResult::Invalid;
			}
			if (Step.Goal == EWarriorTutorialGoal::ItemUsed && Step.GrantItems.IsEmpty())
			{
				Context.AddError(FText::Format(LOCTEXT("NoItem", "{0}: ItemUsed에는 GrantItems가 필요합니다."), StepName));
				Result = EDataValidationResult::Invalid;
			}
			if (Step.HintAfterFailures > 0 && !Step.FailureEventTag.IsValid())
			{
				Context.AddWarning(FText::Format(LOCTEXT("HintNoFailure", "{0}: 힌트가 있지만 FailureEventTag가 없어 실패를 세지 않습니다."), StepName));
			}
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
