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
}

void UWarriorNoticePopupWidget::NativeDestruct()
{
	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
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

void UWarriorNoticePopupWidget::Close()
{
	RemoveFromParent();
	OnClosed.Broadcast();
}

void UWarriorNoticePopupWidget::HandleConfirmClicked()
{
	Close();
}
