#include "WarriorSkillWidget.h"

#include "DataAsset_SkillTree.h"
#include "WarriorSkillEntryWidget.h"
#include "WarriorSkillLibrary.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"
#include "ProjectWarrior/Network/WarriorSkillUnlockSubsystem.h"
#include "ProjectWarrior/ProjectWarrior.h"

#define LOCTEXT_NAMESPACE "WarriorSkill"

UWarriorSkillWidget::UWarriorSkillWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AccountLevelFormat = LOCTEXT("AccountLevelFormat", "계정 레벨 {0}");
	StatPointsFormat = LOCTEXT("StatPointsFormat", "스탯 포인트 {0}");
	UnlockSucceededText = LOCTEXT("UnlockSucceeded", "해금했습니다.");
}

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

	if (Button_Back)
	{
		Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackButtonClicked);
	}

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.AddUniqueDynamic(this, &ThisClass::HandleAccountChanged);
	}
	if (UWarriorSkillUnlockSubsystem* SkillUnlock = UWarriorSkillUnlockSubsystem::Get(this))
	{
		SkillUnlock->OnUnlockCompleted.AddUniqueDynamic(this, &ThisClass::HandleUnlockCompleted);
		SkillUnlock->OnSkillsReceived.AddUniqueDynamic(this, &ThisClass::HandleSkillsReceived);
		// 서버 진행도를 받으면 HandleSkillsReceived가 다시 그린다. 그 전에는 지난 값(또는 로컬 값)으로 그린다
		SkillUnlock->RequestSkills();
	}
	if (Text_Message)
	{
		Text_Message->SetText(FText::GetEmpty());
	}

	Refresh();
}

void UWarriorSkillWidget::NativeDestruct()
{
	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackButtonClicked);
	}

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.RemoveDynamic(this, &ThisClass::HandleAccountChanged);
	}
	if (UWarriorSkillUnlockSubsystem* SkillUnlock = UWarriorSkillUnlockSubsystem::Get(this))
	{
		SkillUnlock->OnUnlockCompleted.RemoveDynamic(this, &ThisClass::HandleUnlockCompleted);
		SkillUnlock->OnSkillsReceived.RemoveDynamic(this, &ThisClass::HandleSkillsReceived);
	}

	Super::NativeDestruct();
}

void UWarriorSkillWidget::Refresh()
{
	FWarriorSkillEvalContext Context;
	UWarriorSkillLibrary::MakeEvalContext(this, Context);
	SkillViews = UWarriorSkillLibrary::EvaluateSkillTree(SkillTree, Context);
	// 로그인 상태에서 받은 서버 진행도가 있으면 그 값이 기준이다(로컬 통계는 이 PC 것이라 서버와 다를 수 있음)
	const UWarriorSkillUnlockSubsystem* SkillUnlock = UWarriorSkillUnlockSubsystem::Get(this);
	if (SkillUnlock && SkillUnlock->HasServerSkills())
	{
		UWarriorSkillLibrary::ApplyServerProgress(SkillViews, SkillUnlock->GetServerSkills());
	}

	UpdateSummaryTexts(Context.AccountLevel, Context.StatPoints);
	UpdateEntries();
	BP_OnSkillsRefreshed(SkillViews, Context.AccountLevel, Context.StatPoints);
}

void UWarriorSkillWidget::UpdateSummaryTexts(const int32 AccountLevel, const int32 StatPoints)
{
	if (Text_AccountLevel)
	{
		Text_AccountLevel->SetText(FText::Format(AccountLevelFormat, AccountLevel));
	}

	if (Text_StatPoints)
	{
		Text_StatPoints->SetText(FText::Format(StatPointsFormat, StatPoints));
	}
}

void UWarriorSkillWidget::UpdateEntries()
{
	if (!SkillList || !EntryWidgetClass)
	{
		return;
	}

	for (int32 Index = 0; Index < SkillViews.Num(); ++Index)
	{
		if (!EntryWidgets.IsValidIndex(Index))
		{
			UWarriorSkillEntryWidget* NewEntry = CreateWidget<UWarriorSkillEntryWidget>(this, EntryWidgetClass);
			if (!NewEntry)
			{
				return;
			}
			SkillList->AddChild(NewEntry);
			EntryWidgets.Add(NewEntry);
		}

		UWarriorSkillEntryWidget* Entry = EntryWidgets[Index];
		Entry->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Entry->SetView(SkillViews[Index], this);
	}

	// DA에서 스킬이 줄어든 경우 남는 줄은 숨긴다.
	for (int32 Index = SkillViews.Num(); Index < EntryWidgets.Num(); ++Index)
	{
		EntryWidgets[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UWarriorSkillWidget::RequestUnlock(FGameplayTag SkillTag)
{
	UWarriorSkillUnlockSubsystem* SkillUnlock = UWarriorSkillUnlockSubsystem::Get(this);
	if (!SkillUnlock || SkillUnlock->IsUnlockInFlight())
	{
		return false;
	}
	if (Text_Message)
	{
		Text_Message->SetText(FText::GetEmpty());
	}

	// 결과는 HandleUnlockCompleted로 온다(로그인하지 않았으면 NOT_LOGGED_IN 실패가 이 호출 안에서 바로 온다).
	// 기다리는 동안 해금 버튼을 끄도록 다시 그린다.
	const bool bSent = SkillUnlock->RequestUnlock(SkillTag);
	if (bSent)
	{
		UpdateEntries();
	}
	return bSent;
}

bool UWarriorSkillWidget::IsUnlocking() const
{
	const UWarriorSkillUnlockSubsystem* SkillUnlock = UWarriorSkillUnlockSubsystem::Get(this);
	return SkillUnlock && SkillUnlock->IsUnlockInFlight();
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

void UWarriorSkillWidget::HandleUnlockCompleted(bool bSuccess, FGameplayTag SkillTag, const FString& ErrorCode, const FText& Message)
{
	// 성공이면 계정 값이 이미 적용돼 화면이 다시 그려졌고, 서버 진행도도 다시 받는 중이다.
	// 실패면 서버 값이 화면과 달랐을 수 있으니 진행도를 다시 받는다(받으면 HandleSkillsReceived가 다시 그림).
	if (!bSuccess)
	{
		if (UWarriorSkillUnlockSubsystem* SkillUnlock = UWarriorSkillUnlockSubsystem::Get(this))
		{
			SkillUnlock->RequestSkills();
		}
	}
	if (Text_Message)
	{
		Text_Message->SetText(bSuccess ? UnlockSucceededText : Message);
	}
	Refresh();
	BP_OnUnlockFinished(SkillTag, bSuccess, Message);
	if (!bSuccess)
	{
		BP_OnUnlockFailed(SkillTag);
	}
}

void UWarriorSkillWidget::HandleSkillsReceived(bool bSuccess)
{
	if (bSuccess)
	{
		Refresh();
	}
}

void UWarriorSkillWidget::HandleBackButtonClicked()
{
	RequestBack();
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
