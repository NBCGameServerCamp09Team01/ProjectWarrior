// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorNoticePopupWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorNoticePopupClosed);

/**
 * 안내 팝업의 베이스. 문구 한 줄과 "확인" 버튼만 있다.
 * 로그인 상태가 끝났을 때(다른 곳에서 로그인, 연결 끊김, 접속 종료) 프론트 컨트롤러가 화면 위에 띄운다.
 * 문구는 C++(UWarriorAuthSubsystem::SessionEndToText)이 정하고, WBP_NoticePopup은 모양만 맡는다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorNoticePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Warrior|Notice")
	void SetMessage(const FText& InMessage);

	//팝업을 닫고 OnClosed로 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Notice")
	void Close();

	//닫혔을 때. 띄운 쪽이 아래 화면으로 입력 포커스를 돌려준다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Notice")
	FOnWarriorNoticePopupClosed OnClosed;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleConfirmClicked();

	//~ Begin 반드시 있어야 하는 위젯 (WBP에 같은 이름으로 둔다)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Message;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;
	//~ End 반드시 있어야 하는 위젯
};
