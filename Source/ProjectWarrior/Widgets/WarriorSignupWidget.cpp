// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSignupWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

#define LOCTEXT_NAMESPACE "WarriorAuth"

void UWarriorSignupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Submit)
	{
		Button_Submit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSubmitClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (EditableTextBox_PasswordConfirm)
	{
		EditableTextBox_PasswordConfirm->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandlePasswordConfirmCommitted);
	}

	//화면을 다시 열 때 이전 비밀번호가 남지 않게 한다
	for (UEditableTextBox* Field : { EditableTextBox_Password.Get(), EditableTextBox_PasswordConfirm.Get() })
	{
		if (Field)
		{
			Field->SetText(FText::GetEmpty());
		}
	}

	ClearMessage();

	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (Auth)
	{
		Auth->OnSignupCompleted.AddUniqueDynamic(this, &ThisClass::HandleSignupCompleted);
	}

	SetBusy(Auth && Auth->IsRequestInFlight());
}

void UWarriorSignupWidget::NativeDestruct()
{
	if (Button_Submit)
	{
		Button_Submit->OnClicked.RemoveDynamic(this, &ThisClass::HandleSubmitClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (EditableTextBox_PasswordConfirm)
	{
		EditableTextBox_PasswordConfirm->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandlePasswordConfirmCommitted);
	}

	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		Auth->OnSignupCompleted.RemoveDynamic(this, &ThisClass::HandleSignupCompleted);
	}

	Super::NativeDestruct();
}

UWidget* UWarriorSignupWidget::GetInitialFocusWidget() const
{
	return EditableTextBox_LoginId;
}

void UWarriorSignupWidget::SetBusy(bool bInBusy)
{
	for (UWidget* Widget : TArray<UWidget*>{ Button_Submit, Button_Back, EditableTextBox_LoginId, EditableTextBox_Nickname, EditableTextBox_Password, EditableTextBox_PasswordConfirm })
	{
		if (Widget)
		{
			Widget->SetIsEnabled(!bInBusy);
		}
	}
}

FText UWarriorSignupWidget::ValidateInputs(FString& OutLoginId, FString& OutPassword, FString& OutNickname) const
{
	OutLoginId = EditableTextBox_LoginId ? EditableTextBox_LoginId->GetText().ToString().TrimStartAndEnd() : FString();
	OutNickname = EditableTextBox_Nickname ? EditableTextBox_Nickname->GetText().ToString().TrimStartAndEnd() : FString();
	OutPassword = EditableTextBox_Password ? EditableTextBox_Password->GetText().ToString() : FString();
	const FString PasswordConfirm = EditableTextBox_PasswordConfirm ? EditableTextBox_PasswordConfirm->GetText().ToString() : OutPassword;

	FText Error = UWarriorAuthSubsystem::ValidateLoginId(OutLoginId);
	if (Error.IsEmpty())
	{
		Error = UWarriorAuthSubsystem::ValidateNickname(OutNickname);
	}
	if (Error.IsEmpty())
	{
		Error = UWarriorAuthSubsystem::ValidatePassword(OutPassword);
	}
	if (Error.IsEmpty() && PasswordConfirm != OutPassword)
	{
		Error = LOCTEXT("PasswordMismatch", "비밀번호가 서로 다릅니다.");
	}

	return Error;
}

void UWarriorSignupWidget::SubmitSignup()
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || Auth->IsRequestInFlight())
	{
		return;
	}

	FString LoginId;
	FString Password;
	FString Nickname;
	const FText Error = ValidateInputs(LoginId, Password, Nickname);
	if (!Error.IsEmpty())
	{
		ShowMessage(Error, true);
		return;
	}

	ShowMessage(LOCTEXT("SigningUp", "가입하는 중..."), false);
	SetBusy(true);
	Auth->RequestSignup(LoginId, Password, Nickname);
}

void UWarriorSignupWidget::HandleSubmitClicked()
{
	SubmitSignup();
}

void UWarriorSignupWidget::HandleBackClicked()
{
	RequestShowScreen(EWarriorFrontScreen::Login);
}

void UWarriorSignupWidget::HandlePasswordConfirmCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	if (InCommitMethod == ETextCommit::OnEnter)
	{
		SubmitSignup();
	}
}

void UWarriorSignupWidget::HandleSignupCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	SetBusy(false);

	if (!bSuccess)
	{
		ShowMessage(Message.IsEmpty() ? LOCTEXT("SignupFailed", "가입하지 못했습니다. 잠시 후 다시 시도해 주세요.") : Message, true);
		return;
	}

	//가입한 아이디는 인증 서브시스템이 기억하고, 로그인 화면이 꺼내서 채운다
	for (UEditableTextBox* Field : { EditableTextBox_LoginId.Get(), EditableTextBox_Nickname.Get(), EditableTextBox_Password.Get(), EditableTextBox_PasswordConfirm.Get() })
	{
		if (Field)
		{
			Field->SetText(FText::GetEmpty());
		}
	}
	ClearMessage();
	RequestShowScreen(EWarriorFrontScreen::Login);
}

#undef LOCTEXT_NAMESPACE
