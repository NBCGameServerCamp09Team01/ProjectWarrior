// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPauseMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorStagePlayerController.h"

UWarriorPauseMenuWidget::UWarriorPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//컨트롤러가 열 때 이 위젯에 포커스를 주므로, 받아서 첫 버튼으로 넘기고 ESC를 받는다
	SetIsFocusable(true);
}

void UWarriorPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Resume)
	{
		Button_Resume->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResumeClicked);
	}

	if (Button_Exit)
	{
		Button_Exit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);
	}
}

void UWarriorPauseMenuWidget::NativeDestruct()
{
	if (Button_Resume)
	{
		Button_Resume->OnClicked.RemoveDynamic(this, &ThisClass::HandleResumeClicked);
	}

	if (Button_Exit)
	{
		Button_Exit->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked);
	}

	Super::NativeDestruct();
}

FReply UWarriorPauseMenuWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	if (Button_Resume && Button_Resume->GetIsEnabled())
	{
		return FReply::Handled().SetUserFocus(Button_Resume->TakeWidget(), InFocusEvent.GetCause());
	}

	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FReply UWarriorPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
		{
			StagePlayerController->HandlePauseBack();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UWarriorPauseMenuWidget::SetLeaving(bool bInLeaving, const FText& InStatus)
{
	if (Button_Resume)
	{
		Button_Resume->SetIsEnabled(!bInLeaving);
	}

	if (Button_Exit)
	{
		Button_Exit->SetIsEnabled(!bInLeaving);
	}

	if (Text_Status)
	{
		Text_Status->SetText(InStatus);
		Text_Status->SetVisibility(bInLeaving ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UWarriorPauseMenuWidget::SetStageInfo(const FText& InText)
{
	if (Text_StageInfo)
	{
		Text_StageInfo->SetText(InText);
	}
}

void UWarriorPauseMenuWidget::HandleResumeClicked()
{
	if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
	{
		StagePlayerController->ClosePause();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Pause] %s: owning player is not AWarriorStagePlayerController. Resume is ignored."), *GetName());
}

void UWarriorPauseMenuWidget::HandleExitClicked()
{
	if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
	{
		StagePlayerController->RequestExitFromPause();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Pause] %s: owning player is not AWarriorStagePlayerController. Exit is ignored."), *GetName());
}
