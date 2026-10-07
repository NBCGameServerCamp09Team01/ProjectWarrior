// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorAuthWidgetBase.h"
#include "WarriorSignupWidget.generated.h"

class UButton;
class UEditableTextBox;

/**
 * 회원가입 화면. 로그인 화면의 "회원가입"으로 연다.
 * 입력 규칙을 먼저 확인하고 UWarriorAuthSubsystem::RequestSignup을 부른다.
 * 가입에 성공하면 로그인 화면으로 돌아간다(명세상 가입은 토큰을 주지 않으므로 이어서 로그인한다).
 * 모양은 WBP_Signup이 정한다(BindWidget 이름을 맞춘다).
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorSignupWidget : public UWarriorAuthWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void SubmitSignup();

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin UWarriorAuthWidgetBase Interface.
	virtual UWidget* GetInitialFocusWidget() const override;
	virtual void SetBusy(bool bInBusy) override;
	//~ End UWarriorAuthWidgetBase Interface

	//입력 규칙을 확인한다. 문제가 있으면 빈 FText가 아닌 문장을 돌려준다
	FText ValidateInputs(FString& OutLoginId, FString& OutPassword, FString& OutNickname, FString& OutEmail) const;

	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleBackClicked();

	//비밀번호 확인 칸에서 Enter를 누르면 가입
	UFUNCTION()
	void HandlePasswordConfirmCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	UFUNCTION()
	void HandleSignupCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

	//~ Begin WBP에 같은 이름으로 꼭 있어야 하는 위젯
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_LoginId;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Nickname;

	//선택 입력. 비워 두면 서버에 보내지 않는다(명세: email은 비워 두거나 null)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Email;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Password;

	//비밀번호 확인. 서버에는 보내지 않고 화면에서만 비교한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_PasswordConfirm;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Submit;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Back;
	//~ End WBP에 꼭 있어야 하는 위젯
};
