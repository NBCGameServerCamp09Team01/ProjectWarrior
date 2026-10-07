// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorAuthWidgetBase.h"
#include "WarriorSignupWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/**
 * 회원가입 화면. 로그인 화면의 "회원가입"으로 연다.
 * 입력 규칙을 먼저 확인하고 UWarriorAuthSubsystem::RequestSignup을 부른다.
 * 가입에 성공하면 로그인 화면으로 돌아간다(명세상 가입은 토큰을 주지 않으므로 이어서 로그인한다).
 * 모양은 WBP_Signup이 정한다(BindWidget 이름을 맞춘다).
 *
 * 오류 표시(명세 "UE 처리"): 칸에 대한 오류는 그 칸 아래 Text_<칸>Error에 보여 준다.
 * - 게임 입력 검사: 모든 칸을 한 번에 검사해 각 칸에 표시
 * - 서버 400 VALIDATION_FAILED(errors[].field), 409 아이디·닉네임 중복
 * 오류가 보이는 동안 그 칸의 규칙 안내(Text_<칸>Help)는 숨긴다. 칸이 없는 오류(서버 장애 등)는 아래 Text_Message에 보여 준다.
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

	//모든 칸의 입력 규칙을 확인한다. 키는 칸 이름(loginId, nickname, email, password, passwordConfirm), 비어 있으면 통과
	TMap<FString, FText> ValidateInputs(FString& OutLoginId, FString& OutPassword, FString& OutNickname, FString& OutEmail) const;

	//칸별 오류를 모두 지우고 규칙 안내를 다시 보인다
	void ClearFieldErrors();

	//칸별 오류를 각 칸 아래에 보이고 오류가 있는 첫 칸으로 포커스를 옮긴다.
	//표시할 칸이 없는 오류(모르는 칸 이름, WBP에 오류 글자가 없음)는 OutUnplaced에 모아 돌려준다
	void ShowFieldErrors(const TMap<FString, FText>& InFieldErrors, FText& OutUnplaced);

	//칸 이름 → 위젯
	UEditableTextBox* FindFieldInput(const FString& InField) const;
	UTextBlock* FindFieldError(const FString& InField) const;
	UTextBlock* FindFieldHelp(const FString& InField) const;

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

	//~ Begin 칸별 오류·안내 (없어도 된다. 없으면 그 칸의 오류는 Text_Message에 보인다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LoginIdError;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NicknameError;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EmailError;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PasswordError;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PasswordConfirmError;

	//규칙 안내("4~20자, 영문 소문자·숫자"). 같은 칸에 오류가 보이는 동안 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LoginIdHelp;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NicknameHelp;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PasswordHelp;
	//~ End 칸별 오류·안내
};
