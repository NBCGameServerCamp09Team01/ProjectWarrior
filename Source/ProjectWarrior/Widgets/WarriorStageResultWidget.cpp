// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageResultWidget.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorStagePlayerController.h"

UWarriorStageResultWidget::UWarriorStageResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//UI 전용 입력 모드에서 이 위젯에 포커스를 주므로 켜 둔다. WBP에서 체크를 빠뜨려도 Non-Focusable 경고가 나지 않는다.
	SetIsFocusable(true);
}

void UWarriorStageResultWidget::SetResult(const FWarriorStageResult& InResult)
{
	Result = InResult;
	BP_OnResultSet(Result);
}

void UWarriorStageResultWidget::RequestRestart()
{
	if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
	{
		StagePlayerController->RestartStage();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: owning player is not AWarriorStagePlayerController. Restart is ignored."), *GetName());
}

void UWarriorStageResultWidget::RequestReturnToMainMenu()
{
	if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
	{
		StagePlayerController->ReturnToMainMenu();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: owning player is not AWarriorStagePlayerController. Return to main menu is ignored."), *GetName());
}
