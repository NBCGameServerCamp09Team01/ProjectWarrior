// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStagePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Audio/WarriorSoundSubsystem.h"
#include "ProjectWarrior/Audio/WarriorSoundTags.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "ProjectWarrior/Widgets/WarriorStageHUDWidget.h"
#include "ProjectWarrior/Widgets/WarriorStageResultWidget.h"

namespace
{
	//뷰포트에 쌓는 순서. 인벤토리 휠(AWarriorPlayerCharacter)이 10, 상점(AWarriorShopActor)이 20을 쓴다.
	const int32 StageHUDZOrder = 0;
	const int32 StageResultZOrder = 30;
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

	GetWorldTimerManager().ClearTimer(ResultRevealTimer);

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
	if (bForceInputMode || Permission.bUIInputMode != bAppliedUIInputMode)
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

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Apply permission for %s. Move: %s, InputMode: %s"),
		*UEnum::GetValueAsString(InState),
		Permission.bCanMove ? TEXT("allowed") : TEXT("locked"),
		Permission.bUIInputMode ? TEXT("UI") : TEXT("Game"));
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

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Show result. %s, Wave %d/%d, PlayTime %.1f s, Reveal in %.1f s"),
		InResult.bCleared ? TEXT("Cleared") : TEXT("Failed"),
		InResult.ReachedWave,
		InResult.TotalWaveCount,
		InResult.PlayTimeSeconds,
		bDelayReveal ? FailedResultDelay : 0.f);

	ResultWidget->SetResult(InResult);

	//결과 위젯은 지금 만들어 두어야 같은 프레임에 오는 통계·보상 알림(OnStageRecorded, OnStageRewarded)을 받는다.
	//그래서 생성은 그대로 두고, 실패일 때 보이는 시점만 늦춰 플레이어 사망 연출을 보여 준다.
	if (bDelayReveal)
	{
		if (!GetWorldTimerManager().IsTimerActive(ResultRevealTimer))
		{
			ResultVisibilityBeforeHide = ResultWidget->GetVisibility();
			ResultWidget->SetVisibility(ESlateVisibility::Hidden);
		}

		GetWorldTimerManager().SetTimer(ResultRevealTimer, this, &ThisClass::RevealResult, FailedResultDelay, false);
	}
	else if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
	{
		//클리어는 바로, 실패는 결과 화면이 보일 때(RevealResult) 결과 음악으로 바꾼다.
		Sound->SetMusicState(InResult.bCleared ? WarriorSoundTags::Music_Result_Cleared : WarriorSoundTags::Music_Result_Failed);
	}
}

void AWarriorStagePlayerController::RevealResult()
{
	if (!ResultWidget)
	{
		return;
	}

	ResultWidget->SetVisibility(ResultVisibilityBeforeHide);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Reveal result after %.1f s"), FailedResultDelay);

	//숨겨 둔 동안 결과 상태 진입(ApplyStatePermission)이 준 포커스는 보이지 않는 위젯에 갔으므로 다시 준다.
	if (bAppliedUIInputMode)
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, ResultWidget);
	}

	//지연은 실패 결과에만 쓰므로 실패 음악으로 바꾼다.
	if (UWarriorSoundSubsystem* Sound = UWarriorSoundSubsystem::Get(this))
	{
		Sound->SetMusicState(WarriorSoundTags::Music_Result_Failed);
	}
}
