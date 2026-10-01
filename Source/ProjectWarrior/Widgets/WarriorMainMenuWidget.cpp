// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
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

	if (Button_Skill)
	{
		Button_Skill->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSkillButtonClicked);
	}

	//성장 화면에서 돌아오면 이 위젯이 다시 구성되므로 투자 뒤의 값이 여기서 반영되고,
	//화면이 떠 있는 동안 값이 바뀌어도(서버 값 적용 등) 알림으로 갱신한다.
	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.AddUniqueDynamic(this, &ThisClass::HandleAccountChanged);
	}
	RefreshAccountSummary();
}

void UWarriorMainMenuWidget::NativeDestruct()
{
	if (Button_Growth)
	{
		Button_Growth->OnClicked.RemoveDynamic(this, &ThisClass::HandleGrowthButtonClicked);
	}

	if (Button_Skill)
	{
		Button_Skill->OnClicked.RemoveDynamic(this, &ThisClass::HandleSkillButtonClicked);
	}

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.RemoveDynamic(this, &ThisClass::HandleAccountChanged);
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

void UWarriorMainMenuWidget::RequestOpenSkill()
{
	if (AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPlayerController->ShowScreen(EWarriorFrontScreen::Skill);
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Front] %s: owning player is not AWarriorFrontPlayerController. Skill screen is not opened."), *GetName());
}

void UWarriorMainMenuWidget::RefreshAccountSummary()
{
	//계정 서브시스템이 없으면(GameInstance 설정이 다른 경우) 새 계정의 값(레벨 1, 포인트 0)을 표시한다.
	const UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this);
	SetAccountSummary(Account ? Account->GetAccountLevel() : 1, Account ? Account->GetStatPoints() : 0);
}

void UWarriorMainMenuWidget::HandleGrowthButtonClicked()
{
	RequestOpenGrowth();
}

void UWarriorMainMenuWidget::HandleSkillButtonClicked()
{
	RequestOpenSkill();
}

void UWarriorMainMenuWidget::HandleAccountChanged(const FWarriorAccountData& InAccountData)
{
	SetAccountSummary(InAccountData.AccountLevel, InAccountData.StatPoints);
}

#undef LOCTEXT_NAMESPACE
