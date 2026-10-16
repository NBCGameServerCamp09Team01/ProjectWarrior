// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorTutorialHUDWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

/**
 * 튜토리얼 화면(계획서 5장). 값과 문구는 AWarriorTutorialPlayerController가 정하고, WBP_TutorialHUD는 모양만 맡는다.
 * 단계 문구·장/단계 진행·횟수·힌트·완료 표시·막힌 조작 안내, 마지막의 완료 화면(메인메뉴/처음부터)을 보인다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorTutorialHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorTutorialHUDWidget(const FObjectInitializer& ObjectInitializer);

	//InChapterNumber·InStepNumber는 1부터
	void SetStep(const FText& InChapterTitle, int32 InChapterNumber, int32 InStepNumber, int32 InStepCount, const FText& InInstruction);
	//InRequired가 1이면 횟수 칸을 숨긴다
	void SetProgress(int32 InCurrent, int32 InRequired);
	//빈 글자면 숨긴다
	void SetHint(const FText& InHint);
	void SetStepDone(bool bInDone);
	//"아직 배우지 않은 조작입니다"를 잠깐 보인다
	void ShowBlockedNotice();
	//완료 화면을 보이고 단계 칸을 숨긴다. 포커스는 컨트롤러가 이 위젯에 준다
	void ShowFinished();

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleRestartClicked();

	void HideBlockedNotice();

	//~ Begin 반드시 있어야 하는 위젯 (WBP에 같은 이름으로 둔다)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Instruction;
	//~ End 반드시 있어야 하는 위젯

	//~ Begin 있으면 쓰는 위젯
	//"3장 방어 · 4/6"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StepInfo;

	//"2 / 3"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Count;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Hint;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Notice;

	//단계 문구·진행을 담은 칸. 완료 화면에서 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Step;

	//완료 표시(체크). 평소에는 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_StepDone;

	//완료 화면. 평소에는 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Finished;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_MainMenu;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Restart;
	//~ End 있으면 쓰는 위젯

	//~ Begin 문구 (WBP 클래스 기본값에서 수정)
	//{0} 장 번호, {1} 장 제목, {2} 단계 번호, {3} 장의 단계 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial|Text")
	FText StepInfoFormat = NSLOCTEXT("WarriorTutorial", "StepInfo", "{0}장 {1} · {2}/{3}");

	//{0} 한 횟수, {1} 해야 할 횟수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial|Text")
	FText CountFormat = NSLOCTEXT("WarriorTutorial", "Count", "{0} / {1}");

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial|Text")
	FText BlockedNoticeText = NSLOCTEXT("WarriorTutorial", "BlockedNotice", "아직 배우지 않은 조작입니다");

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial|Text", meta = (ClampMin = "0.1", Units = "s"))
	float BlockedNoticeSeconds = 1.5f;
	//~ End 문구

private:
	FTimerHandle NoticeTimer;
};
