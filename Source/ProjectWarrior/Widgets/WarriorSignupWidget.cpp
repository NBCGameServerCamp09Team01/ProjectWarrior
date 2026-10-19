// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSignupWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

#define LOCTEXT_NAMESPACE "WarriorAuth"

namespace WarriorSignupFields
{
	//칸 이름은 서버 요청 칸 이름과 같다(passwordConfirm만 화면 전용). 화면 위에서 아래 순서 = 포커스를 옮길 순서
	const TCHAR* LoginId = TEXT("loginId");
	const TCHAR* Nickname = TEXT("nickname");
	const TCHAR* Email = TEXT("email");
	const TCHAR* Password = TEXT("password");
	const TCHAR* PasswordConfirm = TEXT("passwordConfirm");

	const TCHAR* const Order[] = { LoginId, Nickname, Email, Password, PasswordConfirm };
}

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
	ClearFieldErrors();

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
	for (UWidget* Widget : TArray<UWidget*>{ Button_Submit, Button_Back, EditableTextBox_LoginId, EditableTextBox_Nickname, EditableTextBox_Email, EditableTextBox_Password, EditableTextBox_PasswordConfirm })
	{
		if (Widget)
		{
			Widget->SetIsEnabled(!bInBusy);
		}
	}
}

TMap<FString, FText> UWarriorSignupWidget::ValidateInputs(FString& OutLoginId, FString& OutPassword, FString& OutNickname, FString& OutEmail) const
{
	OutLoginId = EditableTextBox_LoginId ? EditableTextBox_LoginId->GetText().ToString().TrimStartAndEnd() : FString();
	OutNickname = EditableTextBox_Nickname ? EditableTextBox_Nickname->GetText().ToString().TrimStartAndEnd() : FString();
	OutEmail = EditableTextBox_Email ? EditableTextBox_Email->GetText().ToString().TrimStartAndEnd() : FString();
	OutPassword = EditableTextBox_Password ? EditableTextBox_Password->GetText().ToString() : FString();
	const FString PasswordConfirm = EditableTextBox_PasswordConfirm ? EditableTextBox_PasswordConfirm->GetText().ToString() : OutPassword;

	//첫 오류에서 멈추지 않고 모든 칸을 검사해 칸마다 하나씩 보여 준다
	TMap<FString, FText> Errors;
	auto AddIfError = [&Errors](const TCHAR* InField, const FText& InError)
	{
		if (!InError.IsEmpty())
		{
			Errors.Add(InField, InError);
		}
	};

	AddIfError(WarriorSignupFields::LoginId, UWarriorAuthSubsystem::ValidateLoginId(OutLoginId));
	AddIfError(WarriorSignupFields::Nickname, UWarriorAuthSubsystem::ValidateNickname(OutNickname));
	AddIfError(WarriorSignupFields::Email, UWarriorAuthSubsystem::ValidateEmail(OutEmail));
	AddIfError(WarriorSignupFields::Password, UWarriorAuthSubsystem::ValidatePassword(OutPassword));

	//비밀번호 자체에 오류가 있으면 확인 칸은 비교하지 않는다(같은 문제를 두 번 말하지 않게)
	if (!Errors.Contains(WarriorSignupFields::Password) && PasswordConfirm != OutPassword)
	{
		Errors.Add(WarriorSignupFields::PasswordConfirm, LOCTEXT("PasswordMismatch", "비밀번호가 서로 다릅니다."));
	}

	return Errors;
}

UEditableTextBox* UWarriorSignupWidget::FindFieldInput(const FString& InField) const
{
	if (InField == WarriorSignupFields::LoginId)         { return EditableTextBox_LoginId; }
	if (InField == WarriorSignupFields::Nickname)        { return EditableTextBox_Nickname; }
	if (InField == WarriorSignupFields::Email)           { return EditableTextBox_Email; }
	if (InField == WarriorSignupFields::Password)        { return EditableTextBox_Password; }
	if (InField == WarriorSignupFields::PasswordConfirm) { return EditableTextBox_PasswordConfirm; }
	return nullptr;
}

UTextBlock* UWarriorSignupWidget::FindFieldError(const FString& InField) const
{
	if (InField == WarriorSignupFields::LoginId)         { return Text_LoginIdError; }
	if (InField == WarriorSignupFields::Nickname)        { return Text_NicknameError; }
	if (InField == WarriorSignupFields::Email)           { return Text_EmailError; }
	if (InField == WarriorSignupFields::Password)        { return Text_PasswordError; }
	if (InField == WarriorSignupFields::PasswordConfirm) { return Text_PasswordConfirmError; }
	return nullptr;
}

UTextBlock* UWarriorSignupWidget::FindFieldHelp(const FString& InField) const
{
	if (InField == WarriorSignupFields::LoginId)  { return Text_LoginIdHelp; }
	if (InField == WarriorSignupFields::Nickname) { return Text_NicknameHelp; }
	if (InField == WarriorSignupFields::Password) { return Text_PasswordHelp; }
	return nullptr;
}

void UWarriorSignupWidget::ClearFieldErrors()
{
	for (const TCHAR* Field : WarriorSignupFields::Order)
	{
		if (UTextBlock* ErrorText = FindFieldError(Field))
		{
			ErrorText->SetText(FText::GetEmpty());
			ErrorText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UTextBlock* HelpText = FindFieldHelp(Field))
		{
			HelpText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UWarriorSignupWidget::ShowFieldErrors(const TMap<FString, FText>& InFieldErrors, FText& OutUnplaced)
{
	TArray<FString> UnplacedLines;
	UEditableTextBox* FirstInput = nullptr;

	//화면 순서대로 표시해 포커스가 가장 위의 오류 칸으로 가게 한다
	for (const TCHAR* Field : WarriorSignupFields::Order)
	{
		const FText* Message = InFieldErrors.Find(Field);
		if (!Message)
		{
			continue;
		}

		UTextBlock* ErrorText = FindFieldError(Field);
		if (!ErrorText)
		{
			UnplacedLines.Add(Message->ToString());
			continue;
		}

		ErrorText->SetText(*Message);
		ErrorText->SetColorAndOpacity(ErrorMessageColor);
		ErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UTextBlock* HelpText = FindFieldHelp(Field))
		{
			HelpText->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (!FirstInput)
		{
			FirstInput = FindFieldInput(Field);
		}
	}

	//모르는 칸 이름(서버가 새 칸을 추가한 경우 등)은 아래 문구 칸으로 보낸다
	for (const TPair<FString, FText>& Pair : InFieldErrors)
	{
		if (!FindFieldInput(Pair.Key))
		{
			UnplacedLines.Add(Pair.Value.ToString());
		}
	}

	OutUnplaced = UnplacedLines.Num() > 0 ? FText::FromString(FString::Join(UnplacedLines, TEXT("\n"))) : FText::GetEmpty();

	if (FirstInput)
	{
		FirstInput->SetKeyboardFocus();
	}
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
	FString Email;

	ClearFieldErrors();
	ClearMessage();

	const TMap<FString, FText> FieldErrors = ValidateInputs(LoginId, Password, Nickname, Email);
	if (FieldErrors.Num() > 0)
	{
		FText Unplaced;
		ShowFieldErrors(FieldErrors, Unplaced);
		ShowMessage(Unplaced, true);
		return;
	}

	ShowMessage(LOCTEXT("SigningUp", "가입하는 중..."), false);
	SetBusy(true);
	Auth->RequestSignup(LoginId, Password, Nickname, Email);
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
		//칸에 대한 오류(400 errors[], 409 중복)는 그 칸 아래에, 나머지(서버 장애·연결 실패 등)는 아래 문구 칸에 보인다
		const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
		const TMap<FString, FText> EmptyErrors;
		const TMap<FString, FText>& FieldErrors = Auth ? Auth->GetLastSignupFieldErrors() : EmptyErrors;

		if (FieldErrors.Num() > 0)
		{
			FText Unplaced;
			ShowFieldErrors(FieldErrors, Unplaced);
			ShowMessage(Unplaced, true);
			return;
		}

		ShowMessage(Message.IsEmpty() ? LOCTEXT("SignupFailed", "가입하지 못했습니다. 잠시 후 다시 시도해 주세요.") : Message, true);
		return;
	}

	ClearFieldErrors();

	//입력 칸을 모두 비운다. 가입했다는 사실만 인증 서브시스템이 기억하고, 로그인 화면이 안내한다
	for (UEditableTextBox* Field : { EditableTextBox_LoginId.Get(), EditableTextBox_Nickname.Get(), EditableTextBox_Email.Get(), EditableTextBox_Password.Get(), EditableTextBox_PasswordConfirm.Get() })
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
