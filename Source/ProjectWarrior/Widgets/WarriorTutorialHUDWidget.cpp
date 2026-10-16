// Fill out your copyright notice in the Description page of Project Settings.

#include "WarriorTutorialHUDWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorTutorialPlayerController.h"

UWarriorTutorialHUDWidget::UWarriorTutorialHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//완료 화면에서 컨트롤러가 포커스를 준다. 받아서 첫 버튼으로 넘긴다
	SetIsFocusable(true);
}

void UWarriorTutorialHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_MainMenu)
	{
		Button_MainMenu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMainMenuClicked);
	}
	if (Button_Restart)
	{
		Button_Restart->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRestartClicked);
	}

	SetHint(FText::GetEmpty());
	SetStepDone(false);
	HideBlockedNotice();
	if (Panel_Finished)
	{
		Panel_Finished->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWarriorTutorialHUDWidget::NativeDestruct()
{
	if (Button_MainMenu)
	{
		Button_MainMenu->OnClicked.RemoveDynamic(this, &ThisClass::HandleMainMenuClicked);
	}
	if (Button_Restart)
	{
		Button_Restart->OnClicked.RemoveDynamic(this, &ThisClass::HandleRestartClicked);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoticeTimer);
	}

	Super::NativeDestruct();
}

FReply UWarriorTutorialHUDWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	if (Button_MainMenu && Button_MainMenu->IsVisible())
	{
		return FReply::Handled().SetUserFocus(Button_MainMenu->TakeWidget(), InFocusEvent.GetCause());
	}
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UWarriorTutorialHUDWidget::SetStep(const FText& InChapterTitle, int32 InChapterNumber, int32 InStepNumber, int32 InStepCount, const FText& InInstruction)
{
	Text_Instruction->SetText(InInstruction);

	if (Text_StepInfo)
	{
		Text_StepInfo->SetText(FText::Format(StepInfoFormat,
			FText::AsNumber(InChapterNumber), InChapterTitle, FText::AsNumber(InStepNumber), FText::AsNumber(InStepCount)));
	}
}

void UWarriorTutorialHUDWidget::SetProgress(int32 InCurrent, int32 InRequired)
{
	if (!Text_Count)
	{
		return;
	}

	if (InRequired <= 1)
	{
		Text_Count->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Text_Count->SetText(FText::Format(CountFormat, FText::AsNumber(InCurrent), FText::AsNumber(InRequired)));
	Text_Count->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWarriorTutorialHUDWidget::SetHint(const FText& InHint)
{
	if (Text_Hint)
	{
		Text_Hint->SetText(InHint);
		Text_Hint->SetVisibility(InHint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UWarriorTutorialHUDWidget::SetStepDone(bool bInDone)
{
	if (Panel_StepDone)
	{
		Panel_StepDone->SetVisibility(bInDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UWarriorTutorialHUDWidget::ShowBlockedNotice()
{
	if (!Text_Notice)
	{
		return;
	}

	Text_Notice->SetText(BlockedNoticeText);
	Text_Notice->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NoticeTimer, this, &ThisClass::HideBlockedNotice, BlockedNoticeSeconds, false);
	}
}

void UWarriorTutorialHUDWidget::HideBlockedNotice()
{
	if (Text_Notice)
	{
		Text_Notice->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWarriorTutorialHUDWidget::ShowFinished()
{
	if (Panel_Step)
	{
		Panel_Step->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetStepDone(false);
	SetHint(FText::GetEmpty());
	HideBlockedNotice();

	if (Panel_Finished)
	{
		Panel_Finished->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UWarriorTutorialHUDWidget::HandleMainMenuClicked()
{
	if (AWarriorTutorialPlayerController* TutorialController = GetOwningPlayer<AWarriorTutorialPlayerController>())
	{
		TutorialController->ReturnToMainMenu();
		return;
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] %s: owning player is not AWarriorTutorialPlayerController."), *GetName());
}

void UWarriorTutorialHUDWidget::HandleRestartClicked()
{
	if (AWarriorTutorialPlayerController* TutorialController = GetOwningPlayer<AWarriorTutorialPlayerController>())
	{
		//옵션 없이 다시 열면 1장부터(AWarriorTutorialGameMode::InitGame)
		TutorialController->RestartStage();
		return;
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] %s: owning player is not AWarriorTutorialPlayerController."), *GetName());
}
