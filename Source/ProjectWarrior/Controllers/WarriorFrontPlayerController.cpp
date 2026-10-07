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
	ShowScreen(EWarriorFrontScreen::Title);
}

void AWarriorFrontPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] %s EndPlay (%s)"),
		*GetClass()->GetName(),
		*UEnum::GetValueAsString(EndPlayReason));

	Super::EndPlay(EndPlayReason);
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
	if (StageLevel.IsNull())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Front] StageLevel is not set."));
		return;
	}

	//UI 전용 입력 설정은 레벨을 옮겨도 뷰포트에 남을 수 있으므로, 이동 전에 게임 입력으로 되돌린다.
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(this);
	bShowMouseCursor = false;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Front] Open stage level %s"), *StageLevel.ToString());

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, StageLevel);
}

void AWarriorFrontPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
