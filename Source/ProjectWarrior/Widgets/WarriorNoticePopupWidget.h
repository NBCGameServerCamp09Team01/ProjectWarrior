// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorNoticePopupWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorNoticePopupClosed);

//버튼을 눌렀다. bConfirmed는 확인(첫 번째) 버튼이면 true, 두 번째 버튼이면 false
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorNoticePopupChoice, bool, bConfirmed);

/**
 * 안내 팝업의 베이스. 문구 한 줄과 "확인" 버튼, 그리고 선택으로 두 번째 버튼이 있다.
 * - 로그인 상태가 끝났을 때(다른 곳에서 로그인, 접속 종료) 프론트 컨트롤러가 화면 위에 띄운다.
 * - 서버 요청 오류(S5)는 UWarriorErrorPresenterSubsystem이 띄운다. "다시 시도 / 게임 종료"처럼 두 버튼을 쓸 때 SetButtons를 부른다.
 * 문구는 C++(문구 표 FWarriorErrorText)이 정하고, WBP_NoticePopup은 모양만 맡는다.
 * 두 번째 버튼 위젯이 WBP에 없으면 확인 버튼 하나로 동작한다(기존 WBP 그대로 쓸 수 있다).
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorNoticePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Warrior|Notice")
	void SetMessage(const FText& InMessage);

	//버튼 문구를 정한다. InSecondaryText가 비어 있으면 두 번째 버튼을 숨긴다.
	//InConfirmText가 비어 있으면 확인 버튼 문구는 WBP에 적힌 그대로 둔다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Notice")
	void SetButtons(const FText& InConfirmText, const FText& InSecondaryText);

	//두 번째 버튼 위젯이 WBP에 있는가
	UFUNCTION(BlueprintPure, Category = "Warrior|Notice")
	bool HasSecondaryButton() const { return Button_Secondary != nullptr; }

	//팝업을 닫고 OnClosed로 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Notice")
	void Close();

	//닫혔을 때. 띄운 쪽이 아래 화면으로 입력 포커스를 돌려준다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Notice")
	FOnWarriorNoticePopupClosed OnClosed;

	//버튼을 눌렀을 때(닫히기 직전). OnClosed보다 먼저 온다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Notice")
	FOnWarriorNoticePopupChoice OnChoice;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleSecondaryClicked();

	//버튼 문구·두 번째 버튼 표시를 위젯에 적용한다
	void ApplyButtons();

	//~ Begin 반드시 있어야 하는 위젯 (WBP에 같은 이름으로 둔다)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Message;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;
	//~ End 반드시 있어야 하는 위젯

	//~ Begin 있으면 쓰는 위젯
	//확인 버튼 안의 문구
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Confirm;

	//두 번째 버튼(예: 게임 종료). 기본은 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Secondary;

	//두 번째 버튼 안의 문구
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Secondary;
	//~ End 있으면 쓰는 위젯

private:
	FText ConfirmText;
	FText SecondaryText;
};
