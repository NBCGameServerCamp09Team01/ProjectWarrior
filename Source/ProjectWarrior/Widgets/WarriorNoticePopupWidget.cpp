// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorNoticePopupWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UWarriorNoticePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	}

	if (Button_Secondary)
	{
		Button_Secondary->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecondaryClicked);
	}

	ApplyButtons();
}

void UWarriorNoticePopupWidget::NativeDestruct()
{
	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}

	if (Button_Secondary)
	{
		Button_Secondary->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecondaryClicked);
	}

	Super::NativeDestruct();
}

void UWarriorNoticePopupWidget::SetMessage(const FText& InMessage)
{
	if (Text_Message)
	{
		Text_Message->SetText(InMessage);
	}
}

void UWarriorNoticePopupWidget::SetButtons(const FText& InConfirmText, const FText& InSecondaryText)
{
	ConfirmText = InConfirmText;
	SecondaryText = InSecondaryText;
	ApplyButtons();
}

void UWarriorNoticePopupWidget::ApplyButtons()
{
	if (Text_Confirm && !ConfirmText.IsEmpty())
	{
		Text_Confirm->SetText(ConfirmText);
	}

	if (Button_Secondary)
	{
		Button_Secondary->SetVisibility(SecondaryText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (Text_Secondary && !SecondaryText.IsEmpty())
	{
		Text_Secondary->SetText(SecondaryText);
	}
}

void UWarriorNoticePopupWidget::Close()
{
	RemoveFromParent();
	OnClosed.Broadcast();
}

void UWarriorNoticePopupWidget::HandleConfirmClicked()
{
	OnChoice.Broadcast(true);
	Close();
}

void UWarriorNoticePopupWidget::HandleSecondaryClicked()
{
	OnChoice.Broadcast(false);
	Close();
}
