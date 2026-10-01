#include "WarriorSkillEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "WarriorSkillWidget.h"

#define LOCTEXT_NAMESPACE "WarriorSkill"

UWarriorSkillEntryWidget::UWarriorSkillEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnlockButtonText = LOCTEXT("Entry_Unlock", "해금");
	UnlockedButtonText = LOCTEXT("Entry_Unlocked", "완료");
}

void UWarriorSkillEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Unlock)
	{
		Button_Unlock->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUnlockButtonClicked);
	}
}

void UWarriorSkillEntryWidget::NativeDestruct()
{
	if (Button_Unlock)
	{
		Button_Unlock->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnlockButtonClicked);
	}

	Super::NativeDestruct();
}

void UWarriorSkillEntryWidget::SetView(const FWarriorSkillView& InView, UWarriorSkillWidget* InOwner)
{
	View = InView;
	OwnerSkillWidget = InOwner;

	if (Text_Name)
	{
		Text_Name->SetText(View.Definition.DisplayName);
	}

	if (Text_Requirement)
	{
		Text_Requirement->SetText(UWarriorSkillWidget::GetRequirementSummary(View));
	}

	if (Text_State)
	{
		Text_State->SetText(UWarriorSkillWidget::GetStateText(View.State));
	}

	// 해금 가능일 때만 누를 수 있다. 해금된 스킬은 "완료"로 표시한다.
	if (Button_Unlock)
	{
		Button_Unlock->SetIsEnabled(View.CanUnlock());
	}

	if (Text_Unlock)
	{
		Text_Unlock->SetText(View.IsUnlocked() ? UnlockedButtonText : UnlockButtonText);
	}

	BP_OnViewSet(View);
}

void UWarriorSkillEntryWidget::HandleUnlockButtonClicked()
{
	if (UWarriorSkillWidget* Owner = OwnerSkillWidget.Get())
	{
		Owner->RequestUnlock(View.Definition.SkillTag);
	}
}

#undef LOCTEXT_NAMESPACE
