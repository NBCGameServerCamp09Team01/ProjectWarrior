// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorAuthWidgetBase.generated.h"

class UTextBlock;
enum class EWarriorFrontScreen : uint8;

/**
 * 로그인·회원가입 화면의 베이스. 로직만 두고 모양은 WBP(WBP_Login, WBP_Signup)가 정한다.
 * 공통 기능: 안내·오류 문구 표시, 첫 입력 칸 포커스, 화면 전환 요청, 요청 중 입력 막기.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorAuthWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorAuthWidgetBase(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface.
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget Interface

	//화면이 포커스를 받으면 넘겨줄 입력 칸 (보통 첫 칸)
	virtual UWidget* GetInitialFocusWidget() const { return nullptr; }

	//안내·오류 문구. bIsError면 ErrorMessageColor, 아니면 InfoMessageColor
	void ShowMessage(const FText& InMessage, bool bIsError);
	void ClearMessage();

	//요청 중에는 버튼과 입력 칸을 막는다
	virtual void SetBusy(bool bInBusy) {}

	//소유 컨트롤러가 프론트 컨트롤러면 화면을 바꾼다
	void RequestShowScreen(EWarriorFrontScreen InScreen);

	//안내·오류 문구를 표시할 글자
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Message;

	//오류 문구 색 (기본: 메인메뉴 스탯 포인트와 같은 강조 빨강)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Auth|Style")
	FSlateColor ErrorMessageColor;

	//안내 문구 색 (기본: 타이틀 글자와 같은 양피지색)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Auth|Style")
	FSlateColor InfoMessageColor;
};
