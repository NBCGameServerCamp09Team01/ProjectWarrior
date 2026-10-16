// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStagePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/Audio/WarriorSoundSubsystem.h"
#include "ProjectWarrior/Audio/WarriorSoundTags.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorPresenterSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"
#include "ProjectWarrior/Shop/WarriorShopActor.h"
#include "ProjectWarrior/Widgets/WarriorNoticePopupWidget.h"
#include "ProjectWarrior/Widgets/WarriorPauseMenuWidget.h"
#include "ProjectWarrior/Widgets/WarriorStageHUDWidget.h"
#include "ProjectWarrior/Widgets/WarriorStageResultWidget.h"

namespace
{
	//뷰포트에 쌓는 순서. 인벤토리 휠(AWarriorPlayerCharacter)이 10, 상점(AWarriorShopActor)이 20, 오류 팝업이 100을 쓴다.
	const int32 StageHUDZOrder = 0;
	const int32 StageResultZOrder = 30;
	const int32 SavingNoticeZOrder = 40;
	const int32 PauseMenuZOrder = 50;
	const int32 ExitConfirmZOrder = 60;
}

void AWarriorStagePlayerController::BeginPlay()
{
	Super::BeginPlay();

	//레벨 전환·재시작 때 컨트롤러가 새로 만들어지는지 확인하기 위한 로그
	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] %s BeginPlay in %s"),
		*GetClass()->GetName(),
		*UGameplayStatics::GetCurrentLevelName(this));

	if (!IsLocalController())
	{
		return;
	}

	//GameState가 없는 레벨이어도 접속이 끊기면 돌아가야 하므로 먼저 구독한다
	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}

	//일시정지 키 컨텍스트. 캐릭터의 기본 컨텍스트(DA_InputConfig)와 따로 둔다
	if (PauseMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(PauseMappingContext, PauseMappingPriority);
		}
	}
	else
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] PauseMappingContext is not set. The pause key is not bound (use Warrior.Stage.Pause)."));
	}

	AWarriorStageGameState* StageGameState = GetWorld()->GetGameState<AWarriorStageGameState>();
	if (!StageGameState)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s is used in a level without AWarriorStageGameState. Stage permissions are not applied."),
			*GetClass()->GetName());
		return;
	}

	BoundGameState = StageGameState;
	StageGameState->OnStageStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStageStateChanged);
	StageGameState->OnStageFinished.AddUniqueDynamic(this, &ThisClass::HandleStageFinished);
	StageGameState->OnWaveChanged.AddUniqueDynamic(this, &ThisClass::HandleWaveChanged);

	//GameMode와 이 컨트롤러의 BeginPlay 순서는 보장되지 않아, 구독 전에 상태가 이미 바뀌었을 수 있다.
	//현재 상태를 한 번 직접 적용하고, 이전 레벨의 입력 설정이 뷰포트에 남아 있을 수 있으므로 입력 모드도 강제로 맞춘다.
	const EWarriorStageState CurrentState = StageGameState->GetStageState();
	ApplyStatePermission(CurrentState, true);

	//음악도 현재 상태로 한 번 맞춘다. 이미 웨이브가 진행 중이면 그 웨이브의 곡으로.
	PlayStageStateSound(CurrentState);
	if (CurrentState == EWarriorStageState::InProgress)
	{
		HandleWaveChanged(StageGameState->GetWaveNumber(), StageGameState->GetTotalWaveCount(), StageGameState->IsBossWave());
	}

	//HUD는 스스로 GameState를 구독해 값을 채우고, 초기화 중과 결과 상태에서는 스스로 숨는다.
	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UWarriorStageHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(StageHUDZOrder);

			if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
			{
				Sound->ApplyButtonSounds(HUDWidget);
			}
		}
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] HUDWidgetClass is not set. Stage HUD is not shown."));
	}
}

void AWarriorStagePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] %s EndPlay (%s)"),
		*GetClass()->GetName(),
		*UEnum::GetValueAsString(EndPlayReason));

	if (AWarriorStageGameState* StageGameState = BoundGameState.Get())
	{
		StageGameState->OnStageStateChanged.RemoveDynamic(this, &ThisClass::HandleStageStateChanged);
		StageGameState->OnStageFinished.RemoveDynamic(this, &ThisClass::HandleStageFinished);
		StageGameState->OnWaveChanged.RemoveDynamic(this, &ThisClass::HandleWaveChanged);
	}
	BoundGameState.Reset();

	//서브시스템은 레벨보다 오래 살므로, 사라지는 컨트롤러를 알림 대상에서 뺀다
	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}
	if (UWarriorErrorPresenterSubsystem* Presenter = UWarriorErrorPresenterSubsystem::Get(this))
	{
		Presenter->OnPopupClosed.RemoveDynamic(this, &ThisClass::HandleErrorPopupClosed);
	}

	//입력 서브시스템은 로컬 플레이어와 함께 레벨보다 오래 산다. 메인메뉴에 일시정지 컨텍스트를 남기지 않는다
	if (PauseMappingContext && IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			InputSubsystem->RemoveMappingContext(PauseMappingContext);
		}
	}

	GetWorldTimerManager().ClearTimer(ResultRevealTimer);
	UnbindResultSubmit();
	HideSavingNotice();

	Super::EndPlay(EndPlayReason);
}

void AWarriorStagePlayerController::RestartStage()
{
	//PIE에서는 패키지 이름에 UEDPIE 접두사가 붙으므로 떼고 연다.
	const FString LevelPath = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Restart stage %s"), *LevelPath);

	//입력 모드는 새로 만들어지는 컨트롤러가 BeginPlay에서 게임 전용으로 맞춘다.
	UGameplayStatics::OpenLevel(this, FName(*LevelPath));
}

void AWarriorStagePlayerController::ReturnToMainMenu()
{
	if (MainMenuLevel.IsNull())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] MainMenuLevel is not set."));
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Open main menu level %s"), *MainMenuLevel.ToString());

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
}

//~ Begin 일시정지

void AWarriorStagePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!PauseAction)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ThisClass::HandlePauseInput);
	}
}

void AWarriorStagePlayerController::HandlePauseInput()
{
	if (!IsPauseOpen())
	{
		TryOpenPause();
	}
}

bool AWarriorStagePlayerController::CanOpenPause(FString& OutReason) const
{
	if (PauseState != EWarriorPauseState::Closed)
	{
		OutReason = TEXT("AlreadyOpen");
		return false;
	}

	//판 상태: 초기화(판 시작 전)·끝 상태(결과를 이미 냄)는 막는다
	const AWarriorStageGameState* StageGameState = BoundGameState.Get();
	const EWarriorStageState StageState = StageGameState ? StageGameState->GetStageState() : EWarriorStageState::None;
	switch (StageState)
	{
	case EWarriorStageState::None:
	case EWarriorStageState::Initializing:
	case EWarriorStageState::StageCleared:
	case EWarriorStageState::StageFailed:
		OutReason = FString::Printf(TEXT("State(%s)"), *UEnum::GetValueAsString(StageState));
		return false;
	default:
		break;
	}

	//마지막 웨이브의 클리어 연출 뒤에는 결과를 낸다. 여기서 나가면 다 깬 판이 포기가 된다(계획서 EX-2)
	if (StageState == EWarriorStageState::WaveCleared)
	{
		const AWarriorStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<AWarriorStageGameMode>();
		if (StageGameMode && StageGameMode->IsCurrentWaveLast())
		{
			OutReason = TEXT("LastWaveCleared");
			return false;
		}
	}

	//사망(사망 ~ StageFailed 사이 포함)·블레이드 모드·전술 모드. 두 모드는 시간 배율을 바꾸고 입력을 따로 묶는다
	if (APawn* ControlledPawn = GetPawn())
	{
		FGameplayTagContainer BlockingTags;
		BlockingTags.AddTag(WarriorGameplayTags::Shared_Status_Death);
		BlockingTags.AddTag(WarriorGameplayTags::Player_Status_BladeMode);
		BlockingTags.AddTag(WarriorGameplayTags::Player_Status_TacticalMode);
		if (UWarriorFunctionLibrary::NativeDoesActorHaveAnyTag(ControlledPawn, BlockingTags))
		{
			OutReason = TEXT("PlayerStatus(Death/BladeMode/TacticalMode)");
			return false;
		}

		//인벤토리 휠은 닫을 때 시간 배율을 1.0·입력 모드를 게임으로 덮어쓴다
		if (const AWarriorPlayerCharacter* PlayerCharacter = Cast<AWarriorPlayerCharacter>(ControlledPawn))
		{
			if (PlayerCharacter->IsInventoryWheelOpen())
			{
				OutReason = TEXT("InventoryWheel");
				return false;
			}
		}
	}

	//상점은 닫을 때 입력 모드를 게임으로 덮어쓴다
	for (TActorIterator<AWarriorShopActor> It(GetWorld()); It; ++It)
	{
		if (It->IsShopOpen())
		{
			OutReason = TEXT("Shop");
			return false;
		}
	}

	//오류 팝업이 떠 있으면 팝업이 먼저(ESC로 팝업의 선택을 대신하지 않는다)
	if (const UWarriorErrorPresenterSubsystem* Presenter = UWarriorErrorPresenterSubsystem::Get(this))
	{
		if (Presenter->IsShowing())
		{
			OutReason = TEXT("ErrorPopup");
			return false;
		}
	}

	return true;
}

bool AWarriorStagePlayerController::TryOpenPause()
{
	FString Reason;
	if (!CanOpenPause(Reason))
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Blocked. reason=%s"), *Reason);
		return false;
	}

	if (!UGameplayStatics::SetGamePaused(this, true))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Pause] SetGamePaused(true) failed."));
		return false;
	}

	PauseState = EWarriorPauseState::Open;

	//누른 채 연 이동 키를 뗀 것이 게임 입력에 전해지지 않아 계속하기 뒤 저절로 움직이지 않게 비운다(계획서 IN-1)
	FlushPressedKeys();

	//멈춘 시간은 플레이 시간에서 뺀다
	if (AWarriorStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<AWarriorStageGameMode>())
	{
		StageGameMode->NotifyPauseStarted();
	}

	ShowPauseMenu();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Open. state=%s"),
		BoundGameState.IsValid() ? *UEnum::GetValueAsString(BoundGameState->GetStageState()) : TEXT("None"));
	return true;
}

void AWarriorStagePlayerController::ClosePause()
{
	if (PauseState == EWarriorPauseState::Closed)
	{
		return;
	}

	//보낸 포기(P4)는 되돌릴 수 없다. 답이 오면 메인메뉴로 간다
	if (PauseState == EWarriorPauseState::Leaving)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Close ignored while leaving."));
		return;
	}

	HidePauseMenu();
	UGameplayStatics::SetGamePaused(this, false);
	PauseState = EWarriorPauseState::Closed;
	FlushPressedKeys();

	if (AWarriorStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<AWarriorStageGameMode>())
	{
		StageGameMode->NotifyPauseEnded();
	}

	//열린 동안 판 상태가 바뀌었을 수 있으므로(웨이브 구성 도착 등) 지금 상태로 입력 모드까지 다시 맞춘다
	if (const AWarriorStageGameState* StageGameState = BoundGameState.Get())
	{
		ApplyStatePermission(StageGameState->GetStageState(), true);
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Close."));
}

void AWarriorStagePlayerController::ShowPauseMenu()
{
	if (!PauseMenuWidgetClass)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] PauseMenuWidgetClass is not set. Paused without a screen."));
		return;
	}

	if (!PauseMenuWidget)
	{
		PauseMenuWidget = CreateWidget<UWarriorPauseMenuWidget>(this, PauseMenuWidgetClass);
		if (!PauseMenuWidget)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Pause] Failed to create the pause menu widget."));
			return;
		}

		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(PauseMenuWidget);
		}
	}

	if (const AWarriorStageGameState* StageGameState = BoundGameState.Get())
	{
		PauseMenuWidget->SetStageInfo(FText::Format(PauseStageInfoFormat,
			FText::AsNumber(StageGameState->GetWaveNumber()), FText::AsNumber(StageGameState->GetTotalWaveCount())));
	}
	PauseMenuWidget->SetLeaving(false);

	if (!PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(PauseMenuZOrder);
	}

	//열린 동안 오류 팝업이 떴다 닫히면 포커스를 되찾는다
	if (UWarriorErrorPresenterSubsystem* Presenter = UWarriorErrorPresenterSubsystem::Get(this))
	{
		Presenter->OnPopupClosed.AddUniqueDynamic(this, &ThisClass::HandleErrorPopupClosed);
	}

	bShowMouseCursor = true;
	FocusPauseWidget();
}

void AWarriorStagePlayerController::HidePauseMenu()
{
	if (UWarriorErrorPresenterSubsystem* Presenter = UWarriorErrorPresenterSubsystem::Get(this))
	{
		Presenter->OnPopupClosed.RemoveDynamic(this, &ThisClass::HandleErrorPopupClosed);
	}

	if (ExitConfirmPopup)
	{
		ExitConfirmPopup->OnChoice.RemoveDynamic(this, &ThisClass::HandleExitChoice);
		if (ExitConfirmPopup->IsInViewport())
		{
			ExitConfirmPopup->RemoveFromParent();
		}
	}

	if (PauseMenuWidget && PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->RemoveFromParent();
	}
}

void AWarriorStagePlayerController::FocusPauseWidget()
{
	UUserWidget* FocusWidget = nullptr;
	//확인 팝업(WBP_NoticePopup)이 포커스를 받지 못하면 일시정지 화면에 둔다. 그러면 ESC는 일시정지 화면이 받아 "취소"가 되고(HandlePauseBack), 팝업 버튼은 마우스로 누른다
	if (PauseState == EWarriorPauseState::ConfirmingExit && ExitConfirmPopup && ExitConfirmPopup->IsInViewport() && ExitConfirmPopup->IsFocusable())
	{
		FocusWidget = ExitConfirmPopup;
	}
	else if (PauseMenuWidget && PauseMenuWidget->IsInViewport())
	{
		FocusWidget = PauseMenuWidget;
	}

	if (FocusWidget)
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, FocusWidget);
	}
}

void AWarriorStagePlayerController::HandlePauseBack()
{
	switch (PauseState)
	{
	case EWarriorPauseState::Open:
		ClosePause();
		break;
	case EWarriorPauseState::ConfirmingExit:
		//일시정지 화면이 ESC를 받았으면 확인 팝업을 취소한 것으로 본다. 버튼으로 고른 게 아니므로 팝업은 여기서 닫는다
		if (ExitConfirmPopup)
		{
			ExitConfirmPopup->OnChoice.RemoveDynamic(this, &ThisClass::HandleExitChoice);
			if (ExitConfirmPopup->IsInViewport())
			{
				ExitConfirmPopup->Close();
			}
		}
		HandleExitChoice(false);
		break;
	default:
		break;
	}
}

void AWarriorStagePlayerController::RequestExitFromPause()
{
	if (PauseState != EWarriorPauseState::Open)
	{
		return;
	}

	if (!ExitConfirmPopupClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Pause] ExitConfirmPopupClass is not set. Leaving without confirmation."));
		BeginLeaving();
		return;
	}

	if (!ExitConfirmPopup)
	{
		ExitConfirmPopup = CreateWidget<UWarriorNoticePopupWidget>(this, ExitConfirmPopupClass);
		if (!ExitConfirmPopup)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Pause] Failed to create the exit confirm popup."));
			return;
		}

		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(ExitConfirmPopup);
		}
	}

	ExitConfirmPopup->SetMessage(ExitConfirmText);
	ExitConfirmPopup->SetButtons(ExitConfirmButtonText, ExitCancelButtonText);
	ExitConfirmPopup->OnChoice.AddUniqueDynamic(this, &ThisClass::HandleExitChoice);
	if (!ExitConfirmPopup->IsInViewport())
	{
		ExitConfirmPopup->AddToViewport(ExitConfirmZOrder);
	}

	PauseState = EWarriorPauseState::ConfirmingExit;
	FocusPauseWidget();
	UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Exit requested. Waiting for confirmation."));
}

void AWarriorStagePlayerController::HandleExitChoice(bool bConfirmed)
{
	if (PauseState != EWarriorPauseState::ConfirmingExit)
	{
		return;
	}

	//버튼으로 골랐으면 팝업이 이 알림 뒤에 스스로 닫는다(UWarriorNoticePopupWidget::Close). 여기서 먼저 내리면 이중 제거 경고가 난다
	if (ExitConfirmPopup)
	{
		ExitConfirmPopup->OnChoice.RemoveDynamic(this, &ThisClass::HandleExitChoice);
	}

	if (!bConfirmed)
	{
		PauseState = EWarriorPauseState::Open;
		FocusPauseWidget();
		UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Exit canceled."));
		return;
	}

	BeginLeaving();
}

void AWarriorStagePlayerController::BeginLeaving()
{
	PauseState = EWarriorPauseState::Leaving;
	if (PauseMenuWidget)
	{
		PauseMenuWidget->SetLeaving(true, LeavingText);
	}
	UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Leaving. Abandoning the play."));

	UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
	if (!StagePlay)
	{
		HandleAbandonFinished(true);
		return;
	}

	//답이 오기 전에 레벨이 바뀔 수 있으므로(세션 끝 등) 약한 참조로 잡는다. 포기할 판이 없으면 이 안에서 바로 온다
	StagePlay->AbandonCurrentStagePlay([WeakThis = TWeakObjectPtr<ThisClass>(this)](bool bEnded)
	{
		if (ThisClass* StrongThis = WeakThis.Get())
		{
			StrongThis->HandleAbandonFinished(bEnded);
		}
	});
}

void AWarriorStagePlayerController::HandleAbandonFinished(bool bEnded)
{
	//세션 끝 등으로 이미 다른 길을 탔으면 무시한다
	if (PauseState != EWarriorPauseState::Leaving)
	{
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Pause] Leave finished. ended=%s"), bEnded ? TEXT("true") : TEXT("false"));

	//포기했는지 모르면 안내한다. 팝업은 레벨이 바뀌어도 메인메뉴에 다시 뜬다(UWarriorErrorPresenterSubsystem)
	if (!bEnded)
	{
		if (UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this))
		{
			FWarriorApiResult Result;
			Result.bOk = false;
			Result.Error.Code = TEXT("STAGE_PLAY_ABANDON_FAILED");
			Result.Rule = FWarriorErrorText::Resolve(Result.Error);
			Api->PresentError(Result);
		}
	}

	//메인메뉴 레벨이 지정되지 않았으면 갇히지 않게 일시정지 화면으로 돌아간다(계획서 EX-3)
	if (MainMenuLevel.IsNull())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Pause] MainMenuLevel is not set. Staying paused."));
		PauseState = EWarriorPauseState::Open;
		if (PauseMenuWidget)
		{
			PauseMenuWidget->SetLeaving(false);
		}
		FocusPauseWidget();
		return;
	}

	ReturnToMainMenu();
}

void AWarriorStagePlayerController::HandleErrorPopupClosed()
{
	if (PauseState == EWarriorPauseState::Open || PauseState == EWarriorPauseState::ConfirmingExit)
	{
		bShowMouseCursor = true;
		FocusPauseWidget();
	}
}

//~ End 일시정지

void AWarriorStagePlayerController::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	//종료 이유는 서브시스템에 남아 있으므로 꺼내지 않는다(프론트 컨트롤러가 BeginPlay에서 꺼내 화면과 팝업을 정한다)
	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Session ended (%s). Returning to the main menu level."),
		*UEnum::GetValueAsString(InReason));

	//일시정지 중이었어도 P4는 보내지 않는다(세션 종료는 다음 로그인 때 묻는다). 늦게 온 포기 답이 메인메뉴로 한 번 더 보내지 않게 닫힌 것으로 둔다
	PauseState = EWarriorPauseState::Closed;
	ReturnToMainMenu();
}

void AWarriorStagePlayerController::HandleStageStateChanged(EWarriorStageState InNewState, EWarriorStageState InOldState)
{
	const bool bResultState = InNewState == EWarriorStageState::StageCleared || InNewState == EWarriorStageState::StageFailed;

	//결과 위젯은 OnStageFinished에서 이미 띄워져 있다. UI 입력 모드로 바꿀 때 거기에 포커스를 준다.
	UUserWidget* FocusWidget = bResultState ? ResultWidget.Get() : nullptr;
	ApplyStatePermission(InNewState, false, FocusWidget);

	PlayStageStateSound(InNewState);
}

void AWarriorStagePlayerController::HandleStageFinished(const FWarriorStageResult& InResult)
{
	ShowResult(InResult);
}

void AWarriorStagePlayerController::HandleWaveChanged(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave)
{
	//웨이브 0은 스테이지 초기화 때의 알림이라 음악을 바꾸지 않는다.
	if (InWaveNumber <= 0)
	{
		return;
	}

	if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
	{
		Sound->SetMusicState(bInBossWave ? WarriorSoundTags::Music_Stage_Boss : WarriorSoundTags::Music_Stage_Normal);
	}
}

void AWarriorStagePlayerController::PlayStageStateSound(EWarriorStageState InState)
{
	switch (InState)
	{
	//웨이브 사이(초기화·준비·쉬는 시간)는 준비 상황. 표에 칸이 없으면 부모(Music.Stage) 칸을 따른다.
	case EWarriorStageState::Initializing:
	case EWarriorStageState::Preparing:
	case EWarriorStageState::Resting:
		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->SetMusicState(WarriorSoundTags::Music_Stage_Prepare);
		}
		break;

	//마지막 웨이브도 WaveCleared를 거쳐 StageCleared로 가므로, 결과 음악과 겹치지 않게 마지막 웨이브는 뺀다.
	case EWarriorStageState::WaveCleared:
		if (const AWarriorStageGameState* StageGameState = BoundGameState.Get())
		{
			if (StageGameState->GetWaveNumber() < StageGameState->GetTotalWaveCount())
			{
				UWarriorSoundSubsystem::PlaySound2D(this, WarriorSoundTags::Sound_UI_Stage_WaveCleared);
			}
		}
		break;

	//웨이브 진행 중 음악은 HandleWaveChanged, 끝 상태의 음악은 ShowResult·RevealResult가 정한다.
	default:
		break;
	}
}

void AWarriorStagePlayerController::ApplyStatePermission(EWarriorStageState InState, bool bForceInputMode, UUserWidget* InFocusWidget)
{
	const AWarriorStageGameState* StageGameState = BoundGameState.Get();
	if (!StageGameState)
	{
		return;
	}

	const FWarriorStagePermission Permission = StageGameState->GetPermission(InState);

	//이동 잠금은 이 클래스만 쓰므로 전환마다 적용한다. SetIgnoreMoveInput은 호출마다 카운트가 쌓이므로 먼저 리셋한다.
	ResetIgnoreMoveInput();
	SetIgnoreMoveInput(!Permission.bCanMove);

	//입력 모드·시점·커서는 인벤토리 휠(UInventoryWheelWidget)도 바꾼다.
	//전환마다 덮으면 열려 있는 휠이 커서와 입력을 잃으므로, UI 입력 모드 값이 바뀔 때(결과 상태 진입·이탈)만 적용한다.
	//일시정지가 열린 동안(예: 웨이브 구성이 도착해 상태가 바뀜)에는 일시정지 화면의 입력을 지키고, 닫을 때(ClosePause) 맞춘다
	if (PauseState == EWarriorPauseState::Closed && (bForceInputMode || Permission.bUIInputMode != bAppliedUIInputMode))
	{
		ResetIgnoreLookInput();
		SetIgnoreLookInput(Permission.bUIInputMode);

		if (Permission.bUIInputMode)
		{
			UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, InFocusWidget);
		}
		else
		{
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(this);
		}

		bShowMouseCursor = Permission.bUIInputMode;
		bAppliedUIInputMode = Permission.bUIInputMode;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Apply permission for %s. Move: %s, InputMode: %s%s"),
		*UEnum::GetValueAsString(InState),
		Permission.bCanMove ? TEXT("allowed") : TEXT("locked"),
		Permission.bUIInputMode ? TEXT("UI") : TEXT("Game"),
		PauseState == EWarriorPauseState::Closed ? TEXT("") : TEXT(" (not applied: pause open)"));
}

void AWarriorStagePlayerController::ShowResult(const FWarriorStageResult& InResult)
{
	if (!ResultWidgetClass)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] ResultWidgetClass is not set. Result screen is not shown."));
		return;
	}

	if (!ResultWidget)
	{
		ResultWidget = CreateWidget<UWarriorStageResultWidget>(this, ResultWidgetClass);
		if (!ResultWidget)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Stage] Failed to create result widget."));
			return;
		}

		ResultWidget->AddToViewport(StageResultZOrder);

		if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
		{
			Sound->ApplyButtonSounds(ResultWidget);
		}
	}

	const bool bDelayReveal = !InResult.bCleared && FailedResultDelay > 0.f;

	//FinishRun이 결과 제출을 먼저 보내고 이 알림을 내므로, 여기서 보면 이미 답을 기다리는 중이다(보내지 못했으면 NotSent로 끝나 있다)
	UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this);
	const bool bWaitSave = StagePlay && StagePlay->IsSubmittingResult();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Show result. %s, Wave %d/%d, PlayTime %.1f s, Reveal in %.1f s, Wait for save: %s"),
		InResult.bCleared ? TEXT("Cleared") : TEXT("Failed"),
		InResult.ReachedWave,
		InResult.TotalWaveCount,
		InResult.PlayTimeSeconds,
		bDelayReveal ? FailedResultDelay : 0.f,
		bWaitSave ? TEXT("yes") : TEXT("no"));

	ResultWidget->SetResult(InResult);
	bResultCleared = InResult.bCleared;
	bResultRevealed = false;
	bResultRevealDelayPending = bDelayReveal;
	bWaitingForResultSave = bWaitSave;

	//결과 위젯은 지금 만들어 두어야 같은 프레임에 오는 통계·보상 알림(OnStageRecorded, OnStageRewarded)을 받는다.
	//그래서 생성은 그대로 두고, 보이는 시점만 늦춘다: 실패면 플레이어 사망 연출 뒤, 그리고 서버가 결과를 받은 뒤.
	if ((bDelayReveal || bWaitSave) && !bResultHidden)
	{
		ResultVisibilityBeforeHide = ResultWidget->GetVisibility();
		ResultWidget->SetVisibility(ESlateVisibility::Hidden);
		bResultHidden = true;
	}

	if (bDelayReveal)
	{
		GetWorldTimerManager().SetTimer(ResultRevealTimer, this, &ThisClass::HandleResultRevealDelayElapsed, FailedResultDelay, false);
	}

	if (bWaitSave && !ResultSubmitHandle.IsValid())
	{
		ResultSubmitHandle = StagePlay->OnResultSubmitFinished.AddUObject(this, &ThisClass::HandleResultSubmitFinished);
	}

	TryRevealResult();
}

void AWarriorStagePlayerController::TryRevealResult()
{
	//사망 연출 중에는 저장 중 문구도 띄우지 않는다(연출이 끝난 뒤 남은 기다림에만 띄운다)
	if (!ResultWidget || bResultRevealed || bResultRevealDelayPending)
	{
		return;
	}

	if (bWaitingForResultSave)
	{
		ShowSavingNotice();
		return;
	}

	HideSavingNotice();
	RevealResult();
}

void AWarriorStagePlayerController::RevealResult()
{
	if (!ResultWidget)
	{
		return;
	}

	bResultRevealed = true;

	if (bResultHidden)
	{
		ResultWidget->SetVisibility(ResultVisibilityBeforeHide);
		bResultHidden = false;

		UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Reveal result."));

		//숨겨 둔 동안 결과 상태 진입(ApplyStatePermission)이 준 포커스는 보이지 않는 위젯에 갔으므로 다시 준다.
		if (bAppliedUIInputMode)
		{
			UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, ResultWidget);
		}
	}

	if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
	{
		Sound->SetMusicState(bResultCleared ? WarriorSoundTags::Music_Result_Cleared : WarriorSoundTags::Music_Result_Failed);
	}
}

void AWarriorStagePlayerController::HandleResultRevealDelayElapsed()
{
	bResultRevealDelayPending = false;
	TryRevealResult();
}

void AWarriorStagePlayerController::HandleResultSubmitFinished(EWarriorResultSubmitState InState)
{
	UnbindResultSubmit();
	bWaitingForResultSave = false;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Result submit finished (%s)."), *UEnum::GetValueAsString(InState));

	//연결 안 됨: 보관 후 종료 안내가 이어진다. 세션 종료: HandleSessionEnded가 메인메뉴로 돌아간다. 어느 쪽이든 결과는 보이지 않는다
	if (InState == EWarriorResultSubmitState::Offline || InState == EWarriorResultSubmitState::SessionEnded)
	{
		HideSavingNotice();
		return;
	}

	//저장·거절(거절 안내 팝업은 StagePlay가 띄운다)·보내지 않음: 결과를 보인다
	TryRevealResult();
}

void AWarriorStagePlayerController::ShowSavingNotice()
{
	if (SavingNotice.IsValid())
	{
		return;
	}

	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}

	//오류 팝업(가운데)과 겹치지 않게 화면 아래쪽에 둔다. 클릭은 통과시킨다
	SavingNotice = SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 120.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			.Padding(FMargin(28.f, 12.f))
			[
				SNew(STextBlock)
				.Text(SavingResultText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
				.ColorAndOpacity(FLinearColor::White)
			]
		];

	Viewport->AddViewportWidgetContent(SavingNotice.ToSharedRef(), SavingNoticeZOrder);
	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Waiting for the server to save the result."));
}

void AWarriorStagePlayerController::HideSavingNotice()
{
	if (!SavingNotice.IsValid())
	{
		return;
	}

	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(SavingNotice.ToSharedRef());
	}
	SavingNotice.Reset();
}

void AWarriorStagePlayerController::UnbindResultSubmit()
{
	if (!ResultSubmitHandle.IsValid())
	{
		return;
	}

	if (UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(this))
	{
		StagePlay->OnResultSubmitFinished.Remove(ResultSubmitHandle);
	}
	ResultSubmitHandle.Reset();
}
