// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAuthWidgetBase.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

UWarriorAuthWidgetBase::UWarriorAuthWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ErrorMessageColor = FSlateColor(FLinearColor(0.70838f, 0.12477f, 0.04092f));
	InfoMessageColor = FSlateColor(FLinearColor(0.84687f, 0.7913f, 0.7011f));

	//프론트 컨트롤러가 화면을 띄울 때 이 위젯에 포커스를 주므로, 받아서 첫 입력 칸으로 넘긴다
	SetIsFocusable(true);
}

FReply UWarriorAuthWidgetBase::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	if (UWidget* FocusWidget = GetInitialFocusWidget())
	{
		return FReply::Handled().SetUserFocus(FocusWidget->TakeWidget(), InFocusEvent.GetCause());
	}

	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UWarriorAuthWidgetBase::ShowMessage(const FText& InMessage, bool bIsError)
{
	if (!Text_Message)
	{
		return;
	}

	Text_Message->SetText(InMessage);
	Text_Message->SetColorAndOpacity(bIsError ? ErrorMessageColor : InfoMessageColor);
	Text_Message->SetVisibility(InMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UWarriorAuthWidgetBase::ClearMessage()
{
	ShowMessage(FText::GetEmpty(), false);
}

void UWarriorAuthWidgetBase::RequestShowScreen(EWarriorFrontScreen InScreen)
{
	if (AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPlayerController->ShowScreen(InScreen);
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Auth] %s: owning player is not AWarriorFrontPlayerController. Screen is not changed."), *GetName());
}
