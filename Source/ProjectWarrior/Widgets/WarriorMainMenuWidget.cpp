// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

#define LOCTEXT_NAMESPACE "WarriorMainMenu"

UWarriorMainMenuWidget::UWarriorMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AccountLevelFormat = LOCTEXT("AccountLevelFormat", "Lv. {0}");
	StatPointsFormat = LOCTEXT("StatPointsFormat", "스탯 포인트 {0}");
}

void UWarriorMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Growth)
	{
		Button_Growth->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGrowthButtonClicked);
	}

	//성장 화면에서 돌아오면 이 위젯이 다시 구성되므로, 투자 뒤의 값이 여기서 반영된다.
	RefreshAccountSummary();
}

void UWarriorMainMenuWidget::NativeDestruct()
{
	if (Button_Growth)
	{
		Button_Growth->OnClicked.RemoveDynamic(this, &ThisClass::HandleGrowthButtonClicked);
	}

	Super::NativeDestruct();
}

void UWarriorMainMenuWidget::SetAccountSummary(int32 InAccountLevel, int32 InStatPoints)
{
	if (Text_AccountLevel)
	{
		Text_AccountLevel->SetText(FText::Format(AccountLevelFormat, InAccountLevel));
	}

	if (Text_StatPoints)
	{
		Text_StatPoints->SetText(FText::Format(StatPointsFormat, InStatPoints));
	}
}

void UWarriorMainMenuWidget::RequestOpenGrowth()
{
	if (AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPlayerController->ShowScreen(EWarriorFrontScreen::Growth);
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Front] %s: owning player is not AWarriorFrontPlayerController. Growth screen is not opened."), *GetName());
}

void UWarriorMainMenuWidget::RefreshAccountSummary()
{
	//GameInstance 연결 지점: 계정 레벨과 남은 스탯 포인트를 GameInstance에서 읽어 넘긴다.
	//연결 전까지는 새 계정의 값(레벨 1, 포인트 0)을 표시한다.
	SetAccountSummary(1, 0);
}

void UWarriorMainMenuWidget::HandleGrowthButtonClicked()
{
	RequestOpenGrowth();
}

#undef LOCTEXT_NAMESPACE
