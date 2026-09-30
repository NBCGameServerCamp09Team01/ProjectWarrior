// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStagePlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"

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

	//GameMode와 이 컨트롤러의 BeginPlay 순서는 보장되지 않아, 구독 전에 상태가 이미 바뀌었을 수 있다.
	//현재 상태를 한 번 직접 적용하고, 이전 레벨의 입력 설정이 뷰포트에 남아 있을 수 있으므로 입력 모드도 강제로 맞춘다.
	ApplyStatePermission(StageGameState->GetStageState(), true);
}

void AWarriorStagePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] %s EndPlay (%s)"),
		*GetClass()->GetName(),
		*UEnum::GetValueAsString(EndPlayReason));

	if (AWarriorStageGameState* StageGameState = BoundGameState.Get())
	{
		StageGameState->OnStageStateChanged.RemoveDynamic(this, &ThisClass::HandleStageStateChanged);
	}
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void AWarriorStagePlayerController::HandleStageStateChanged(EWarriorStageState InNewState, EWarriorStageState InOldState)
{
	ApplyStatePermission(InNewState);
}

void AWarriorStagePlayerController::ApplyStatePermission(EWarriorStageState InState, bool bForceInputMode)
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
			UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this);
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
