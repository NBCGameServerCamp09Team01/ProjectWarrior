// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorAuthWidgetBase.h"
#include "WarriorLoginWidget.generated.h"

class UButton;
class UEditableTextBox;

/**
 * 로그인 화면. 타이틀의 "게임 시작" 뒤, 로그인하지 않았으면 이 화면이 뜬다(AWarriorFrontPlayerController::ShowScreen).
 * 아이디·비밀번호로 UWarriorAuthSubsystem::RequestLogin을 부르고, 성공하면 메인메뉴로 간다.
 * 회원가입 버튼은 회원가입 화면을, 뒤로 버튼은 타이틀을 연다.
 * 모양은 WBP_Login이 정한다(BindWidget 이름을 맞춘다).
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorLoginWidget : public UWarriorAuthWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void SubmitLogin();

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin UWarriorAuthWidgetBase Interface.
	virtual UWidget* GetInitialFocusWidget() const override;
	virtual void SetBusy(bool bInBusy) override;
	//~ End UWarriorAuthWidgetBase Interface

	UFUNCTION()
	void HandleLoginClicked();

	UFUNCTION()
	void HandleSignupClicked();

	UFUNCTION()
	void HandleBackClicked();

	//비밀번호 칸에서 Enter를 누르면 로그인
	UFUNCTION()
	void HandlePasswordCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	UFUNCTION()
	void HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

	//429 잠김: 남은 초 동안 로그인 버튼을 막고, 시간이 지나면 다시 켠다(서버도 잠김을 지키므로 화면 안내용)
	void LockLoginButton(int32 InSeconds);
	void HandleLoginLockExpired();

	FTimerHandle LoginLockTimer;

	bool bLoginLocked = false;

	bool bBusy = false;

	//~ Begin WBP에 같은 이름으로 꼭 있어야 하는 위젯
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_LoginId;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableTextBox_Password;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Login;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Signup;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Back;
	//~ End WBP에 꼭 있어야 하는 위젯
};
