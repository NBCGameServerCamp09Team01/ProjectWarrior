#include "WarriorGrowthWidget.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

void UWarriorGrowthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.AddUniqueDynamic(this, &ThisClass::HandleAccountChanged);
		BP_OnAccountRefreshed(Account->GetAccountData());
	}
}

void UWarriorGrowthWidget::NativeDestruct()
{
	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.RemoveDynamic(this, &ThisClass::HandleAccountChanged);
	}

	Super::NativeDestruct();
}

bool UWarriorGrowthWidget::RequestInvest(FGameplayTag StatTag)
{
	UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this);
	//성공하면 OnAccountChanged가 와서 화면이 갱신된다
	return Account && Account->InvestStatPoint(StatTag, 1);
}

void UWarriorGrowthWidget::RequestBack()
{
	if (AWarriorFrontPlayerController* FrontPC = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPC->ShowScreen(EWarriorFrontScreen::MainMenu);
	}
}

void UWarriorGrowthWidget::HandleAccountChanged(const FWarriorAccountData& InAccountData)
{
	BP_OnAccountRefreshed(InAccountData);
}