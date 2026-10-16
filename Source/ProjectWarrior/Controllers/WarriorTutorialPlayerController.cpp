// Fill out your copyright notice in the Description page of Project Settings.

#include "WarriorTutorialPlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Audio/WarriorSoundSubsystem.h"
#include "ProjectWarrior/GameModes/WarriorTutorialGameMode.h"
#include "ProjectWarrior/Tutorial/DataAsset_TutorialSteps.h"
#include "ProjectWarrior/Widgets/WarriorTutorialHUDWidget.h"

namespace
{
	//스테이지 HUD와 같은 층. 일시정지 화면(50)·오류 팝업(100)이 위에 온다
	const int32 TutorialHUDZOrder = 0;
}

AWarriorTutorialPlayerController::AWarriorTutorialPlayerController()
{
	//튜토리얼은 잃는 것(보상·진행·이어하기)이 없다. 스테이지 문구를 쓰지 않는다
	ExitConfirmText = NSLOCTEXT("WarriorTutorial", "ExitConfirm", "튜토리얼을 그만두고 메인메뉴로 갈까요?");
	LeavingText = NSLOCTEXT("WarriorTutorial", "Leaving", "메인메뉴로 가는 중…");
}

void AWarriorTutorialPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	//메인메뉴의 UI 입력 설정이 뷰포트에 남아 있을 수 있다(스테이지는 ApplyStatePermission이 강제로 맞춘다)
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(this);
	bShowMouseCursor = false;

	if (AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode())
	{
		TutorialChangedHandle = TutorialGameMode->OnTutorialChanged.AddUObject(this, &ThisClass::HandleTutorialChanged);
		AbilityBlockedHandle = TutorialGameMode->OnAbilityBlocked.AddUObject(this, &ThisClass::HandleAbilityBlocked);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] %s is used without AWarriorTutorialGameMode."), *GetClass()->GetName());
	}

	if (TutorialHUDWidgetClass)
	{
		TutorialHUDWidget = CreateWidget<UWarriorTutorialHUDWidget>(this, TutorialHUDWidgetClass);
		if (TutorialHUDWidget)
		{
			TutorialHUDWidget->AddToViewport(TutorialHUDZOrder);
			if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
			{
				Sound->ApplyButtonSounds(TutorialHUDWidget);
			}
		}
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] TutorialHUDWidgetClass is not set. Tutorial HUD is not shown."));
	}

	//GameMode가 이미 첫 단계를 시작했을 수 있다(RestartPlayer와 이 BeginPlay의 순서는 보장되지 않음)
	RefreshHUD();
}

void AWarriorTutorialPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode())
	{
		TutorialGameMode->OnTutorialChanged.Remove(TutorialChangedHandle);
		TutorialGameMode->OnAbilityBlocked.Remove(AbilityBlockedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

AWarriorTutorialGameMode* AWarriorTutorialPlayerController::GetTutorialGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<AWarriorTutorialGameMode>() : nullptr;
}

void AWarriorTutorialPlayerController::HandleTutorialChanged()
{
	RefreshHUD();
}

void AWarriorTutorialPlayerController::HandleAbilityBlocked()
{
	if (TutorialHUDWidget)
	{
		TutorialHUDWidget->ShowBlockedNotice();
	}
}

void AWarriorTutorialPlayerController::RefreshHUD()
{
	const AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode();
	if (!TutorialHUDWidget || !TutorialGameMode)
	{
		return;
	}

	const EWarriorTutorialState State = TutorialGameMode->GetTutorialState();
	if (State == EWarriorTutorialState::Finished)
	{
		TutorialHUDWidget->ShowFinished();
		ApplyTutorialInputMode();
		return;
	}

	const UDataAsset_TutorialSteps* Steps = TutorialGameMode->GetTutorialSteps();
	const FWarriorTutorialStep* Step = TutorialGameMode->GetCurrentStep();
	if (!Steps || !Step)
	{
		return;
	}

	const FWarriorTutorialChapter& Chapter = Steps->GetChapters()[TutorialGameMode->GetChapterIndex()];

	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), GetKeyText(*Step));
	const FText Instruction = FText::Format(Step->Instruction, Args);

	TutorialHUDWidget->SetStep(Chapter.Title, TutorialGameMode->GetChapterIndex() + 1,
		TutorialGameMode->GetStepIndex() + 1, Chapter.Steps.Num(), Instruction);
	TutorialHUDWidget->SetProgress(TutorialGameMode->GetStepProgress(), Step->RequiredCount);
	TutorialHUDWidget->SetHint(TutorialGameMode->IsHintShown() ? Step->HintText : FText::GetEmpty());
	TutorialHUDWidget->SetStepDone(State == EWarriorTutorialState::StepDone);
}

FText AWarriorTutorialPlayerController::GetKeyText(const FWarriorTutorialStep& InStep) const
{
	if (InStep.KeyAction)
	{
		if (const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			//무기를 든 뒤에 붙는 컨텍스트의 키는 그 전에는 비어 있을 수 있다 → 대체 문구
			const TArray<FKey> Keys = InputSubsystem->QueryKeysMappedToAction(InStep.KeyAction);
			if (!Keys.IsEmpty())
			{
				return Keys[0].GetDisplayName();
			}
		}
	}
	return InStep.FallbackKeyText;
}

void AWarriorTutorialPlayerController::ApplyTutorialInputMode()
{
	if (IsPauseOpen())
	{
		return;
	}

	const AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode();
	const bool bFinished = TutorialGameMode && TutorialGameMode->GetTutorialState() == EWarriorTutorialState::Finished;

	ResetIgnoreLookInput();
	ResetIgnoreMoveInput();
	SetIgnoreLookInput(bFinished);
	SetIgnoreMoveInput(bFinished);
	if (bFinished)
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, TutorialHUDWidget);
	}
	else
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(this);
	}
	bShowMouseCursor = bFinished;

	if (bFinished != bAppliedFinishedInput)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Input mode %s."), bFinished ? TEXT("UI (finished)") : TEXT("Game"));
		bAppliedFinishedInput = bFinished;
	}
}

//~ Begin 일시정지

bool AWarriorTutorialPlayerController::IsPauseAllowedByProgress(FString& OutReason) const
{
	const AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode();
	const EWarriorTutorialState State = TutorialGameMode ? TutorialGameMode->GetTutorialState() : EWarriorTutorialState::Loading;
	//시작 전과 완료 화면에서는 열지 않는다(완료 화면에 메인메뉴 버튼이 있다)
	if (State == EWarriorTutorialState::Loading || State == EWarriorTutorialState::Finished)
	{
		OutReason = FString::Printf(TEXT("Tutorial(%s)"), *UEnum::GetValueAsString(State));
		return false;
	}
	return true;
}

void AWarriorTutorialPlayerController::RestoreInputAfterPause()
{
	ApplyTutorialInputMode();
}

FText AWarriorTutorialPlayerController::GetPauseInfoText() const
{
	const AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode();
	const UDataAsset_TutorialSteps* Steps = TutorialGameMode ? TutorialGameMode->GetTutorialSteps() : nullptr;
	if (!Steps || !Steps->GetChapters().IsValidIndex(TutorialGameMode->GetChapterIndex()))
	{
		return FText::GetEmpty();
	}
	return FText::Format(PauseTutorialInfoFormat,
		FText::AsNumber(TutorialGameMode->GetChapterIndex() + 1), Steps->GetChapters()[TutorialGameMode->GetChapterIndex()].Title);
}

bool AWarriorTutorialPlayerController::CanSkipFromPause() const
{
	const AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode();
	return TutorialGameMode && TutorialGameMode->CanSkipCurrentStep();
}

void AWarriorTutorialPlayerController::SkipFromPause()
{
	if (!CanSkipFromPause())
	{
		return;
	}

	//닫은 뒤 넘긴다. 다음 단계 준비(구역 이동·적 부르기)는 멈추지 않은 월드에서 한다
	ClosePause();
	if (AWarriorTutorialGameMode* TutorialGameMode = GetTutorialGameMode())
	{
		TutorialGameMode->SkipCurrentStep();
	}
}

//~ End 일시정지
