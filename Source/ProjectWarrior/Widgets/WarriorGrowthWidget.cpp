#include "WarriorGrowthWidget.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"
#include "ProjectWarrior/Network/WarriorStatInvestSubsystem.h"

UWarriorGrowthWidget::UWarriorGrowthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//UI 전용 입력 모드에서 이 위젯에 포커스를 주므로 켜 둔다. WBP에서 체크를 빠뜨려도 Non-Focusable 경고가 나지 않는다.
	SetIsFocusable(true);
}

void UWarriorGrowthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.AddUniqueDynamic(this, &ThisClass::HandleAccountChanged);
	}
	if (UWarriorStatInvestSubsystem* StatInvest = UWarriorStatInvestSubsystem::Get(this))
	{
		StatInvest->OnInvestCompleted.AddUniqueDynamic(this, &ThisClass::HandleInvestCompleted);
	}

	PendingPoints.Reset();
	RefreshPreview();
}

void UWarriorGrowthWidget::NativeDestruct()
{
	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.RemoveDynamic(this, &ThisClass::HandleAccountChanged);
	}
	if (UWarriorStatInvestSubsystem* StatInvest = UWarriorStatInvestSubsystem::Get(this))
	{
		StatInvest->OnInvestCompleted.RemoveDynamic(this, &ThisClass::HandleInvestCompleted);
	}

	Super::NativeDestruct();
}

bool UWarriorGrowthWidget::RequestInvest(FGameplayTag StatTag)
{
	const UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this);
	if (!Account || IsApplying())
	{
		return false;
	}

	//미리보기 기준: 남은 포인트 1 이상, 이 스탯이 최대치 미만
	int32 PendingSum = 0;
	for (const TPair<FGameplayTag, int32>& Pair : PendingPoints)
	{
		PendingSum += Pair.Value;
	}
	const int32 Pending = GetPendingPoints(StatTag);
	const int32 Max = Account->GetStatMaxPoints(StatTag);
	if (Account->GetAccountData().StatPoints - PendingSum < 1 || Max <= 0
		|| Account->GetInvestedPoints(StatTag) + Pending + 1 > Max)
	{
		return false;
	}

	PendingPoints.Add(StatTag, Pending + 1);
	if (PendingPoints[StatTag] == 0)
	{
		PendingPoints.Remove(StatTag);
	}
	RefreshPreview();
	return true;
}

bool UWarriorGrowthWidget::RemovePending(FGameplayTag StatTag)
{
	const UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this);
	if (!Account || IsApplying())
	{
		return false;
	}

	//미리보기 기준: 이 스탯이 0보다 크면 내릴 수 있다(저장된 값 아래로도, 포인트를 돌려받음)
	const int32 Pending = GetPendingPoints(StatTag);
	if (Account->GetInvestedPoints(StatTag) + Pending - 1 < 0)
	{
		return false;
	}

	PendingPoints.Add(StatTag, Pending - 1);
	if (PendingPoints[StatTag] == 0)
	{
		PendingPoints.Remove(StatTag);
	}
	RefreshPreview();
	return true;
}

bool UWarriorGrowthWidget::ApplyPending()
{
	UWarriorStatInvestSubsystem* StatInvest = UWarriorStatInvestSubsystem::Get(this);
	if (!StatInvest || IsApplying() || PendingPoints.IsEmpty())
	{
		return false;
	}
	//결과는 HandleInvestCompleted로 온다(로그인하지 않았으면 NOT_LOGGED_IN 실패가 이 호출 안에서 바로 온다)
	return StatInvest->RequestInvest(PendingPoints);
}

void UWarriorGrowthWidget::CancelPending()
{
	if (IsApplying())
	{
		return;
	}
	PendingPoints.Reset();
	RefreshPreview();
}

int32 UWarriorGrowthWidget::GetPendingPoints(FGameplayTag StatTag) const
{
	const int32* Pending = PendingPoints.Find(StatTag);
	return Pending ? *Pending : 0;
}

bool UWarriorGrowthWidget::IsApplying() const
{
	const UWarriorStatInvestSubsystem* StatInvest = UWarriorStatInvestSubsystem::Get(this);
	return StatInvest && StatInvest->IsInvestInFlight();
}

void UWarriorGrowthWidget::RequestBack()
{
	PendingPoints.Reset();
	if (AWarriorFrontPlayerController* FrontPC = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPC->ShowScreen(EWarriorFrontScreen::MainMenu);
	}
}

void UWarriorGrowthWidget::HandleAccountChanged(const FWarriorAccountData& InAccountData)
{
	RefreshPreview();
}

void UWarriorGrowthWidget::HandleInvestCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	//성공이면 계정 값(서버 값)이 이미 적용됐다. 미리보기를 비운다. 거절이면 미리보기를 두고 서버 값 위에 다시 그린다
	if (bSuccess)
	{
		PendingPoints.Reset();
	}
	RefreshPreview();
	BP_OnInvestFinished(bSuccess, Message);
}

void UWarriorGrowthWidget::RefreshPreview()
{
	const UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this);
	if (!Account)
	{
		return;
	}

	FWarriorAccountData Preview = Account->GetAccountData();
	for (const TPair<FGameplayTag, int32>& Pair : PendingPoints)
	{
		const int32* Invested = Preview.InvestedStats.Find(Pair.Key);
		Preview.InvestedStats.Add(Pair.Key, (Invested ? *Invested : 0) + Pair.Value);
		Preview.StatPoints -= Pair.Value;
	}
	BP_OnAccountRefreshed(Preview);
}
