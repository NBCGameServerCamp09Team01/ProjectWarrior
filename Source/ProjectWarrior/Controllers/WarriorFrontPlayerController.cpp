// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorFrontPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Audio/WarriorSoundSubsystem.h"
#include "ProjectWarrior/Audio/WarriorSoundTags.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorStageListSubsystem.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"
#include "ProjectWarrior/Widgets/WarriorNoticePopupWidget.h"
#include "ProjectWarrior/Widgets/WarriorRankingWidgets.h"
#include "ProjectWarrior/Widgets/WarriorStageSelectWidgets.h"

namespace
{
	//화면 위젯(기본 0)보다 위에 띄운다
	const int32 NoticePopupZOrder = 50;

	//화면 위, 안내 팝업 아래
	const int32 ReconnectIndicatorZOrder = 40;
	//화면 위젯(0)보다 위, 안내 팝업보다 아래
	const int32 StageSelectPopupZOrder = 20;
	//메인메뉴 랭킹 팝업(스테이지 선택 팝업과 같은 층, 둘이 함께 뜨지 않음)
	const int32 RankingPopupZOrder = 20;
}

void AWarriorFrontPlayerController::BeginPlay()
{
	Super::BeginPlay();

	//레벨 전환 시 컨트롤러가 바뀌는지 확인하기 위한 로그
	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] %s BeginPlay in %s"),
		*GetClass()->GetName(),
		*UGameplayStatics::GetCurrentLevelName(this));

	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = true;

	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (Auth)
	{
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}

	//스테이지에서 다시 연결하던 중에 이 레벨로 왔을 수도 있으므로 지금 상태를 한 번 맞춘다
	if (UWarriorRealtimeSubsystem* Realtime = UWarriorRealtimeSubsystem::Get(this))
	{
		Realtime->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRealtimeStateChanged);
		HandleRealtimeStateChanged(Realtime->GetState());
	}

	//스테이지에서 로그인 상태가 끝나 이 레벨로 돌아왔다면, 그 이유에 맞는 화면과 안내를 보인다
	EWarriorSessionEndReason PendingReason;
	FText PendingMessage;
	if (Auth && Auth->ConsumePendingSessionEnd(PendingReason, PendingMessage))
	{
		ApplySessionEnd(PendingReason, PendingMessage);
		return;
	}

	//스테이지에서 메인메뉴로 돌아왔으면(로그인 상태가 남아 있음) 타이틀을 건너뛰고 메인메뉴. 게임을 처음 켜면 로그인 전이라 타이틀
	ShowScreen(Auth && Auth->IsLoggedIn() ? EWarriorFrontScreen::MainMenu : EWarriorFrontScreen::Title);
}

void AWarriorFrontPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] %s EndPlay (%s)"),
		*GetClass()->GetName(),
		*UEnum::GetValueAsString(EndPlayReason));

	//서브시스템은 레벨보다 오래 살므로, 사라지는 컨트롤러를 알림 대상에서 뺀다
	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}

	if (UWarriorRealtimeSubsystem* Realtime = UWarriorRealtimeSubsystem::Get(this))
	{
		Realtime->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleRealtimeStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AWarriorFrontPlayerController::HandleRealtimeStateChanged(EWarriorRealtimeState InState)
{
	SetReconnectIndicatorVisible(InState == EWarriorRealtimeState::Reconnecting);
}

void AWarriorFrontPlayerController::SetReconnectIndicatorVisible(bool bVisible)
{
	if (!bVisible)
	{
		if (ReconnectIndicator)
		{
			ReconnectIndicator->RemoveFromParent();
		}
		return;
	}

	if (!ReconnectIndicatorClass)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Front] Reconnecting to the server (ReconnectIndicatorClass is not set, no indicator shown)."));
		return;
	}

	if (!ReconnectIndicator)
	{
		ReconnectIndicator = CreateWidget<UUserWidget>(this, ReconnectIndicatorClass);
		if (!ReconnectIndicator)
		{
			return;
		}
		//표시만 한다: 클릭을 막지 않고, 입력 모드·포커스는 건드리지 않는다
		ReconnectIndicator->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (!ReconnectIndicator->IsInViewport())
	{
		ReconnectIndicator->AddToViewport(ReconnectIndicatorZOrder);
	}
}

void AWarriorFrontPlayerController::ShowScreen(EWarriorFrontScreen InScreen)
{
	//메인메뉴는 로그인한 뒤에만 연다. 타이틀의 "게임 시작"(WBP_Title)이 MainMenu를 부르므로 여기서 로그인 화면으로 돌린다.
	if (InScreen == EWarriorFrontScreen::MainMenu)
	{
		const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
		if (Auth && !Auth->IsLoggedIn())
		{
			InScreen = EWarriorFrontScreen::Login;
		}
	}

	if (InScreen == CurrentScreen)
	{
		return;
	}

	//스테이지 선택 팝업은 메인메뉴 위에만 뜬다. 다른 화면으로 가면(로그인 상태 끝 등) 닫는다
	if (StageSelectPopup && StageSelectPopup->IsInViewport())
	{
		StageSelectPopup->RemoveFromParent();
	}

	//다음 위젯부터 확보한다. 실패하면 현재 화면을 그대로 둔다(빈 화면 방지).
	UUserWidget* NextWidget = ScreenWidgets.FindRef(InScreen);
	const bool bCreatedNow = !NextWidget;
	if (!NextWidget)
	{
		const TSubclassOf<UUserWidget> WidgetClass = ScreenWidgetClasses.FindRef(InScreen);
		if (!WidgetClass)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Front] No widget class for %s"), *UEnum::GetValueAsString(InScreen));
			return;
		}

		NextWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (!NextWidget)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Front] Failed to create widget for %s"), *UEnum::GetValueAsString(InScreen));
			return;
		}

		ScreenWidgets.Add(InScreen, NextWidget);
	}

	if (UUserWidget* CurrentWidget = ScreenWidgets.FindRef(CurrentScreen))
	{
		CurrentWidget->RemoveFromParent();
	}

	NextWidget->AddToViewport();
	const EWarriorFrontScreen PreviousScreen = CurrentScreen;
	CurrentScreen = InScreen;

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, NextWidget);

	//위젯은 재사용하므로 버튼 소리는 처음 띄울 때 한 번만 넣는다(화면 구성 뒤라 위젯 그래프가 만든 버튼까지 포함).
	if (bCreatedNow)
	{
		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(NextWidget);
		}
	}

	PlayScreenSound(PreviousScreen, InScreen);
}

void AWarriorFrontPlayerController::PlayScreenSound(EWarriorFrontScreen InPreviousScreen, EWarriorFrontScreen InNextScreen)
{
	UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this);
	if (!Sound)
	{
		return;
	}

	switch (InNextScreen)
	{
	//로그인·회원가입은 타이틀에서 이어지는 화면이라 같은 상황으로 둔다.
	case EWarriorFrontScreen::Title:
	case EWarriorFrontScreen::Login:
	case EWarriorFrontScreen::Signup:
		Sound->SetMusicState(WarriorSoundTags::Music_Front_Title);
		break;

	//성장·스킬 화면은 메인메뉴에서 여는 화면이라 같은 상황으로 둔다.
	case EWarriorFrontScreen::MainMenu:
	case EWarriorFrontScreen::Growth:
	case EWarriorFrontScreen::Skill:
		Sound->SetMusicState(WarriorSoundTags::Music_Front_MainMenu);
		break;

	default:
		break;
	}

	//"게임 시작" 소리: 타이틀(이미 로그인함) 또는 로그인 화면에서 메인메뉴로 들어갈 때
	if ((InPreviousScreen == EWarriorFrontScreen::Title || InPreviousScreen == EWarriorFrontScreen::Login)
		&& InNextScreen == EWarriorFrontScreen::MainMenu)
	{
		UWarriorSoundSubsystem::PlaySound2D(this, WarriorSoundTags::Sound_UI_Front_Start);
	}
}

void AWarriorFrontPlayerController::StartStage()
{
	//스테이지 선택 팝업에서 고른 스테이지의 레벨. 고른 것이 없으면(예전 길) BP의 StageLevel
	TSoftObjectPtr<UWorld> LevelToOpen = StageLevel;
	if (const UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this))
	{
		if (LaunchSubsystem->HasLaunchInfo() && !LaunchSubsystem->GetLaunchInfo().Level.IsNull())
		{
			LevelToOpen = LaunchSubsystem->GetLaunchInfo().Level;
		}
	}

	if (LevelToOpen.IsNull())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Front] No stage level to open (no selected stage and StageLevel is not set)."));
		return;
	}

	//UI 전용 입력 설정은 레벨을 옮겨도 뷰포트에 남을 수 있으므로, 이동 전에 게임 입력으로 되돌린다.
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(this);
	bShowMouseCursor = false;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] Open stage level %s"), *LevelToOpen.ToString());

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, LevelToOpen);
}

void AWarriorFrontPlayerController::OpenStageSelect()
{
	if (!StageSelectPopupClass)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Front] StageSelectPopupClass is not set. Stage select is not opened."));
		return;
	}

	if (!StageSelectPopup)
	{
		StageSelectPopup = CreateWidget<UWarriorStageSelectPopupWidget>(this, StageSelectPopupClass);
		if (!StageSelectPopup)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Front] Failed to create stage select popup."));
			return;
		}

		StageSelectPopup->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleStageSelectClosed);

		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(StageSelectPopup);
		}
	}

	if (!StageSelectPopup->IsInViewport())
	{
		StageSelectPopup->AddToViewport(StageSelectPopupZOrder);
	}

	StageSelectPopup->Refresh();
	FocusTopWidget();
}

void AWarriorFrontPlayerController::OpenRanking()
{
	if (!RankingPopupClass)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Front] RankingPopupClass is not set. Ranking is not opened."));
		return;
	}

	if (!RankingPopup)
	{
		RankingPopup = CreateWidget<UWarriorRankingPopupWidget>(this, RankingPopupClass);
		if (!RankingPopup)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Front] Failed to create ranking popup."));
			return;
		}

		RankingPopup->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleRankingClosed);

		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(RankingPopup);
		}
	}

	if (!RankingPopup->IsInViewport())
	{
		RankingPopup->AddToViewport(RankingPopupZOrder);
	}

	RankingPopup->Open();
	FocusTopWidget();
}

void AWarriorFrontPlayerController::HandleRankingClosed()
{
	FocusTopWidget();
}

void AWarriorFrontPlayerController::EnterStage(FName InStageId)
{
	//열린 스테이지만 들어간다. 판정은 서버 status(목록 서브시스템이 받은 값)
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const FWarriorStageListEntry* Stage = StageList ? StageList->FindStage(InStageId) : nullptr;
	if (!Stage || !Stage->bCanEnter)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Front] Enter ignored. Stage %s is not open or not in the list."), *InStageId.ToString());
		if (StageSelectPopup)
		{
			StageSelectPopup->HandleEnterFailed();
		}
		return;
	}

	UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this);
	if (!LaunchSubsystem)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Front] No stage launch subsystem. Stage %s is not entered."), *InStageId.ToString());
		if (StageSelectPopup)
		{
			StageSelectPopup->HandleEnterFailed();
		}
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] Enter stage %s"), *InStageId.ToString());

	LaunchSubsystem->SelectStage(InStageId);
	StartStage();
}

void AWarriorFrontPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AWarriorFrontPlayerController::Logout()
{
	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		Auth->RequestLogout();
	}
}

void AWarriorFrontPlayerController::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	//이 컨트롤러가 알림을 직접 받았으므로 레벨 이동용으로 남겨 둔 이유는 지운다(다음 BeginPlay에서 다시 보이지 않게)
	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		EWarriorSessionEndReason PendingReason;
		FText PendingMessage;
		Auth->ConsumePendingSessionEnd(PendingReason, PendingMessage);
	}

	ApplySessionEnd(InReason, InMessage);
}

void AWarriorFrontPlayerController::ApplySessionEnd(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] Session ended (%s)."), *UEnum::GetValueAsString(InReason));

	switch (InReason)
	{
	case EWarriorSessionEndReason::Expired:
	case EWarriorSessionEndReason::InvalidToken:
		ShowScreen(EWarriorFrontScreen::Login);
		break;

	default:
		//LoggedOut, Replaced, ConnectionLost
		ShowScreen(EWarriorFrontScreen::Title);
		break;
	}

	//화면을 옮긴 뒤에 띄워야 팝업이 위에 오고 입력 포커스도 팝업으로 간다. LoggedOut은 문구가 없다
	if (!InMessage.IsEmpty())
	{
		ShowNotice(InMessage);
	}
}

void AWarriorFrontPlayerController::ShowNotice(const FText& InMessage)
{
	if (!NoticePopupClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Front] NoticePopupClass is not set. Notice not shown: %s"), *InMessage.ToString());
		return;
	}

	if (!NoticePopup)
	{
		NoticePopup = CreateWidget<UWarriorNoticePopupWidget>(this, NoticePopupClass);
		if (!NoticePopup)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Front] Failed to create notice popup."));
			return;
		}

		NoticePopup->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleNoticeClosed);

		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(NoticePopup);
		}
	}

	NoticePopup->SetMessage(InMessage);
	if (!NoticePopup->IsInViewport())
	{
		NoticePopup->AddToViewport(NoticePopupZOrder);
	}

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, NoticePopup);
}

void AWarriorFrontPlayerController::HandleNoticeClosed()
{
	FocusTopWidget();
}

void AWarriorFrontPlayerController::HandleStageSelectClosed()
{
	FocusTopWidget();
}

void AWarriorFrontPlayerController::FocusTopWidget()
{
	UUserWidget* FocusWidget = nullptr;
	if (NoticePopup && NoticePopup->IsInViewport())
	{
		FocusWidget = NoticePopup;
	}
	else if (StageSelectPopup && StageSelectPopup->IsInViewport())
	{
		FocusWidget = StageSelectPopup;
	}
	else if (RankingPopup && RankingPopup->IsInViewport())
	{
		FocusWidget = RankingPopup;
	}
	else
	{
		FocusWidget = ScreenWidgets.FindRef(CurrentScreen);
	}

	if (FocusWidget)
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, FocusWidget);
	}
}
