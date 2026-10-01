#include "WarriorSkillWidget.h"

#include "DataAsset_SkillTree.h"
#include "WarriorSkillLibrary.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"
#include "ProjectWarrior/ProjectWarrior.h"

#define LOCTEXT_NAMESPACE "WarriorSkill"

void UWarriorSkillWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UI 입력 모드에서 이 위젯에 포커스를 주므로 포커스를 받을 수 있게 한다 (Non-Focusable 에러 방지).
	SetIsFocusable(true);
}

void UWarriorSkillWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!SkillTree)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Skill] %s has no SkillTree. Set DA_SkillTree in the widget defaults."), *GetClass()->GetName());
	}

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.AddUniqueDynamic(this, &ThisClass::HandleAccountChanged);
	}
	Refresh();
}

void UWarriorSkillWidget::NativeDestruct()
{
	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.RemoveDynamic(this, &ThisClass::HandleAccountChanged);
	}

	Super::NativeDestruct();
}

void UWarriorSkillWidget::Refresh()
{
	FWarriorSkillEvalContext Context;
	UWarriorSkillLibrary::MakeEvalContext(this, Context);
	SkillViews = UWarriorSkillLibrary::EvaluateSkillTree(SkillTree, Context);

	BP_OnSkillsRefreshed(SkillViews, Context.AccountLevel, Context.StatPoints);
}

bool UWarriorSkillWidget::RequestUnlock(FGameplayTag SkillTag)
{
	// 성공하면 계정이 OnAccountChanged를 방송해서 화면이 갱신된다.
	if (UWarriorSkillLibrary::TryUnlockSkill(this, SkillTree, SkillTag))
	{
		return true;
	}

	// 실패해도 그사이 값이 바뀌었을 수 있으니 다시 그린다.
	Refresh();
	BP_OnUnlockFailed(SkillTag);
	return false;
}

void UWarriorSkillWidget::RequestBack()
{
	if (AWarriorFrontPlayerController* FrontPC = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPC->ShowScreen(EWarriorFrontScreen::MainMenu);
	}
}

bool UWarriorSkillWidget::FindSkillView(FGameplayTag SkillTag, FWarriorSkillView& OutView) const
{
	const FWarriorSkillView* Found = SkillViews.FindByPredicate([&SkillTag](const FWarriorSkillView& Candidate)
	{
		return Candidate.Definition.SkillTag == SkillTag;
	});
	if (!Found)
	{
		return false;
	}
	OutView = *Found;
	return true;
}

void UWarriorSkillWidget::HandleAccountChanged(const FWarriorAccountData& InAccountData)
{
	Refresh();
}

FText UWarriorSkillWidget::GetStateText(const EWarriorSkillState State)
{
	switch (State)
	{
	case EWarriorSkillState::Unlocked:   return LOCTEXT("State_Unlocked", "해금됨");
	case EWarriorSkillState::Unlockable: return LOCTEXT("State_Unlockable", "해금 가능");
	case EWarriorSkillState::NeedPoints: return LOCTEXT("State_NeedPoints", "포인트 부족");
	case EWarriorSkillState::Locked:     return LOCTEXT("State_Locked", "잠김");
	}
	return FText::GetEmpty();
}

FText UWarriorSkillWidget::GetRequirementLine(const FWarriorSkillRequirementProgress& Progress)
{
	if (!Progress.bHasValue)
	{
		return FText::Format(LOCTEXT("Line_NoValue", "{0} (기록 없음)"), Progress.Text);
	}

	const FText Current = FText::AsNumber(FMath::FloorToInt(Progress.Current));
	const FText Target = FText::AsNumber(FMath::RoundToInt(Progress.Target));
	return Progress.bUpperBound
		? FText::Format(LOCTEXT("Line_Upper", "{0} ({1} / {2} 이하)"), Progress.Text, Current, Target)
		: FText::Format(LOCTEXT("Line_Lower", "{0} ({1} / {2})"), Progress.Text, Current, Target);
}

FText UWarriorSkillWidget::GetRequirementSummary(const FWarriorSkillView& View)
{
	TArray<FText> Parts;
	for (const FGameplayTag& Prerequisite : View.Definition.Prerequisites)
	{
		// 선행 스킬 이름은 태그의 마지막 부분으로 표시한다 (Account.Skill.HeavyAttack → HeavyAttack). 디자인 단계에서 DA 이름으로 바꾼다.
		FString TagName = Prerequisite.ToString();
		int32 DotIndex = INDEX_NONE;
		if (TagName.FindLastChar(TEXT('.'), DotIndex))
		{
			TagName.RightChopInline(DotIndex + 1);
		}
		Parts.Add(FText::Format(LOCTEXT("Summary_Prereq", "선행: {0}"), FText::FromString(TagName)));
	}
	for (const FWarriorSkillRequirementProgress& Progress : View.Requirements)
	{
		Parts.Add(GetRequirementLine(Progress));
	}

	if (Parts.IsEmpty())
	{
		return LOCTEXT("Summary_None", "조건 없음");
	}
	return FText::Join(FText::FromString(TEXT(" · ")), Parts);
}

#undef LOCTEXT_NAMESPACE
