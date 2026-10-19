// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorStagePlayerController.h"
#include "WarriorTutorialPlayerController.generated.h"

class AWarriorTutorialGameMode;
class UWarriorTutorialHUDWidget;
struct FWarriorTutorialStep;

/**
 * 튜토리얼 레벨의 PlayerController(계획서 2장 A). 스테이지 컨트롤러의 일시정지·나가기·세션 끝·메인메뉴 이동을 그대로 쓰고,
 * 판 상태 대신 튜토리얼 진행 상태로 일시정지를 막고, 튜토리얼 HUD를 그린다.
 * - 나가기: 서버 플레이가 없으므로 포기(P4)를 부르지 않는다(ShouldAbandonStagePlayOnLeave)
 * - 일시정지 메뉴의 "이 단계 건너뛰기" → AWarriorTutorialGameMode::SkipCurrentStep
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorTutorialPlayerController : public AWarriorStagePlayerController
{
	GENERATED_BODY()

public:
	AWarriorTutorialPlayerController();

	//~ Begin AWarriorStagePlayerController 일시정지
	virtual bool CanSkipFromPause() const override;
	virtual void SkipFromPause() override;
	//~ End AWarriorStagePlayerController 일시정지

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin AWarriorStagePlayerController 자식이 바꾸는 곳
	virtual bool IsPauseAllowedByProgress(FString& OutReason) const override;
	virtual void RestoreInputAfterPause() override;
	virtual FText GetPauseInfoText() const override;
	virtual bool ShouldAbandonStagePlayOnLeave() const override { return false; }
	virtual bool UsesStageGameState() const override { return false; }
	//~ End AWarriorStagePlayerController 자식이 바꾸는 곳

	//튜토리얼 화면(BP_TutorialPlayerController에서 WBP_TutorialHUD 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial")
	TSubclassOf<UWarriorTutorialHUDWidget> TutorialHUDWidgetClass;

	//일시정지 화면의 정보 줄. {0} 장 번호, {1} 장 제목
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial")
	FText PauseTutorialInfoFormat = NSLOCTEXT("WarriorTutorial", "PauseInfo", "튜토리얼 {0}장 {1}");

	//한 입력에 키가 여러 개면 이만큼까지 이어서 보인다(이동 W/A/S/D)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "1"))
	int32 MaxKeysShown = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial")
	FString KeySeparator = TEXT("/");

private:
	AWarriorTutorialGameMode* GetTutorialGameMode() const;

	void HandleTutorialChanged();
	void HandleAbilityBlocked();

	//지금 진행 상태를 화면에 옮긴다
	void RefreshHUD();
	//진행 상태에 맞는 입력 모드(완료 화면이면 UI, 아니면 게임). 일시정지 중에는 바꾸지 않는다
	void ApplyTutorialInputMode();

	//{Key}에 넣을 글자: KeyAction에 지금 매핑된 키(패드 제외, 여러 개면 KeySeparator로 이음), 없으면 FallbackKeyText
	FText GetKeyText(const FWarriorTutorialStep& InStep) const;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorTutorialHUDWidget> TutorialHUDWidget;

	FDelegateHandle TutorialChangedHandle;
	FDelegateHandle AbilityBlockedHandle;
	bool bAppliedFinishedInput = false;
};
