// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorPlayerController.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStagePlayerController.generated.h"

class AWarriorStageGameState;

/**
 * 스테이지 레벨의 PlayerController.
 * 스테이지 상태가 바뀔 때마다 GameState의 상태별 권한 표(AWarriorStageGameState::GetPermission)를
 * 이동 잠금·시점 잠금·입력 모드·커서에 적용한다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorStagePlayerController : public AWarriorPlayerController
{
	GENERATED_BODY()

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//GameState의 OnStageStateChanged에 연결
	UFUNCTION()
	void HandleStageStateChanged(EWarriorStageState InNewState, EWarriorStageState InOldState);

	//InState의 권한을 적용한다.
	//입력 모드·시점·커서는 UI 입력 모드 값이 바뀔 때만 적용하고, bForceInputMode가 true면 같아도 다시 적용한다.
	void ApplyStatePermission(EWarriorStageState InState, bool bForceInputMode = false);

private:
	TWeakObjectPtr<AWarriorStageGameState> BoundGameState;

	//마지막으로 적용한 UI 입력 모드 값
	bool bAppliedUIInputMode = false;
};
