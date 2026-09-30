// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorPlayerController.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStagePlayerController.generated.h"

class AWarriorStageGameState;
class UUserWidget;
class UWarriorStageHUDWidget;
class UWarriorStageResultWidget;

/**
 * 스테이지 레벨의 PlayerController.
 * 스테이지 상태가 바뀔 때마다 GameState의 상태별 권한 표(AWarriorStageGameState::GetPermission)를
 * 이동 잠금·시점 잠금·입력 모드·커서에 적용한다.
 * 스테이지 HUD를 만들고, 끝 상태(StageCleared·StageFailed)에서는 결과 화면을 띄운다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorStagePlayerController : public AWarriorPlayerController
{
	GENERATED_BODY()

public:
	//현재 스테이지 레벨을 다시 연다 (결과 화면 "다시 하기")
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RestartStage();

	//MainMenuLevel로 이동한다 (결과 화면 "메인메뉴")
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void ReturnToMainMenu();

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
	//InFocusWidget은 UI 입력 모드로 바꿀 때 포커스를 줄 위젯
	void ApplyStatePermission(EWarriorStageState InState, bool bForceInputMode = false, UUserWidget* InFocusWidget = nullptr);

	//결과 위젯을 만들어(처음 한 번) 결과를 채우고 띄운다. 띄우지 못하면 nullptr
	UWarriorStageResultWidget* ShowResult(EWarriorStageState InState);

	//스테이지 HUD (BP_StagePlayerController에서 WBP_StageHUD 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorStageHUDWidget> HUDWidgetClass;

	//결과 화면 (BP에서 WBP_StageResult 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorStageResultWidget> ResultWidgetClass;

	//결과 화면의 "메인메뉴"로 이동할 레벨 (BP에서 L_Front 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	TWeakObjectPtr<AWarriorStageGameState> BoundGameState;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageResultWidget> ResultWidget;

	//마지막으로 적용한 UI 입력 모드 값
	bool bAppliedUIInputMode = false;
};
