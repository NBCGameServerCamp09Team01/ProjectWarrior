// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Chat/WarriorChatSettings.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

#define LOCTEXT_NAMESPACE "WarriorMainMenu"

UWarriorMainMenuWidget::UWarriorMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AccountLevelFormat = LOCTEXT("AccountLevelFormat", "Lv. {0}");
	StatPointsFormat = LOCTEXT("StatPointsFormat", "스탯 포인트 {0}");
	WelcomeFormat = LOCTEXT("WelcomeFormat", "{0} 영웅님, 환영합니다.");
	WelcomeNoNameText = LOCTEXT("WelcomeNoName", "영웅님, 환영합니다.");
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

	if (Button_Ranking)
	{
		Button_Ranking->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRankingButtonClicked);
	}

	if (Button_Logout)
	{
		Button_Logout->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogoutButtonClicked);
	}

	//다른 계정으로 다시 로그인하면 이 위젯이 다시 구성되므로 여기서 읽으면 된다
	RefreshWelcome();

	//성장 화면에서 돌아오면 이 위젯이 다시 구성되므로 투자 뒤의 값이 여기서 반영되고,
	//화면이 떠 있는 동안 값이 바뀌어도(서버 값 적용 등) 알림으로 갱신한다.
	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnAccountChanged.AddUniqueDynamic(this, &ThisClass::HandleAccountChanged);
	}
	RefreshAccountSummary();

	AddChatPanel();
}

void UWarriorMainMenuWidget::AddChatPanel()
{
	//이미 넣었으면(화면을 다시 보일 때) 그대로 둔다. 채팅 내용은 채팅 서브시스템이 들고 있어 창을 다시 만들 필요가 없다
	if (!Slot_Chat || Slot_Chat->GetContent())
	{
		return;
	}

	//클래스를 WBP_MainMenu가 아니라 설정(DefaultGame.ini)에서 읽어, 메인메뉴 에셋을 고치지 않고 채팅 창을 바꾼다(채팅 계획서 C-7)
	const UWarriorChatSettings* Settings = UWarriorChatSettings::Get();
	if (!Settings || Settings->ChatPanelClass.IsNull())
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Chat] No chat panel class. The main menu has no chat window."));
		return;
	}

	UClass* PanelClass = Settings->ChatPanelClass.LoadSynchronous();
	if (!PanelClass)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Chat] Chat panel class %s could not be loaded. The main menu has no chat window."),
			*Settings->ChatPanelClass.ToString());
		return;
	}

	if (UUserWidget* Panel = CreateWidget<UUserWidget>(GetOwningPlayer(), PanelClass))
	{
		Slot_Chat->SetContent(Panel);
	}
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

	if (Button_Ranking)
	{
		Button_Ranking->OnClicked.RemoveDynamic(this, &ThisClass::HandleRankingButtonClicked);
	}

	if (Button_Logout)
	{
		Button_Logout->OnClicked.RemoveDynamic(this, &ThisClass::HandleLogoutButtonClicked);
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

void UWarriorMainMenuWidget::RefreshWelcome()
{
	if (!Text_Welcome)
	{
		return;
	}

	//닉네임은 서버 명세에 추가되면 채워진다. 그 전까지는 이름 없는 문구를 쓴다
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	const FString Nickname = Auth ? Auth->GetAccount().Nickname : FString();
	Text_Welcome->SetText(Nickname.IsEmpty()
		? WelcomeNoNameText
		: FText::Format(WelcomeFormat, FText::FromString(Nickname)));
}

void UWarriorMainMenuWidget::HandleGrowthButtonClicked()
{
	RequestOpenGrowth();
}

void UWarriorMainMenuWidget::HandleSkillButtonClicked()
{
	RequestOpenSkill();
}

void UWarriorMainMenuWidget::RequestOpenRanking()
{
	if (AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPlayerController->OpenRanking();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Front] %s: owning player is not AWarriorFrontPlayerController. Ranking is not opened."), *GetName());
}

void UWarriorMainMenuWidget::HandleRankingButtonClicked()
{
	RequestOpenRanking();
}

void UWarriorMainMenuWidget::HandleLogoutButtonClicked()
{
	if (AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPlayerController->Logout();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Front] %s: owning player is not AWarriorFrontPlayerController. Logout is not requested."), *GetName());
}

void UWarriorMainMenuWidget::HandleAccountChanged(const FWarriorAccountData& InAccountData)
{
	SetAccountSummary(InAccountData.AccountLevel, InAccountData.StatPoints);
}

#undef LOCTEXT_NAMESPACE
