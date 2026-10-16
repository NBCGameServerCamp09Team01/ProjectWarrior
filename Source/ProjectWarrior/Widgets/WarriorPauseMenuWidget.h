// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorPauseMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * 스테이지 일시정지 화면의 베이스. 동작과 문구는 C++(AWarriorStagePlayerController)이 정하고, WBP_PauseMenu는 모양만 맡는다.
 * - "계속하기"·"나가기" 버튼은 소유 컨트롤러의 ClosePause·RequestExitFromPause를 부른다.
 * - ESC는 이 위젯이 직접 받는다(열린 동안은 UI 입력 모드라 게임 입력이 오지 않는다). 컨트롤러의 HandlePauseBack으로 넘긴다.
 * - 포커스를 받으면 "계속하기"로 넘긴다(되돌릴 수 있는 쪽에 먼저).
 * 계획서: docs_local/pause-exit-plan-draft-1016.md 4·5장
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	//포기 요청(P4)을 기다리는 동안 버튼을 막고 InStatus를 보인다. false면 버튼을 풀고 문구를 숨긴다
	void SetLeaving(bool bInLeaving, const FText& InStatus = FText::GetEmpty());

	//스테이지 정보 줄(예: "웨이브 3 / 5"). Text_StageInfo가 없으면 무시한다
	void SetStageInfo(const FText& InText);

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleExitClicked();

	//~ Begin 반드시 있어야 하는 위젯 (WBP에 같은 이름으로 둔다)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Resume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Exit;
	//~ End 반드시 있어야 하는 위젯

	//~ Begin 있으면 쓰는 위젯
	//"스테이지를 포기하는 중…". 평소에는 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	//"웨이브 3 / 5"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageInfo;
	//~ End 있으면 쓰는 위젯
};
