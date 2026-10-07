// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorLoginWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"

#define LOCTEXT_NAMESPACE "WarriorAuth"

void UWarriorLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Login)
	{
		Button_Login->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoginClicked);
	}

	if (Button_Signup)
	{
		Button_Signup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSignupClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (EditableTextBox_Password)
	{
		EditableTextBox_Password->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandlePasswordCommitted);
		//화면을 다시 열 때 이전 비밀번호가 남지 않게 한다
		EditableTextBox_Password->SetText(FText::GetEmpty());
	}

	ClearMessage();

	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (Auth)
	{
		Auth->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleLoginCompleted);

		//회원가입 화면에서 막 가입하고 넘어왔으면 아이디를 채우고 안내한다
		FString SignedUpLoginId;
		if (Auth->ConsumeRecentSignupLoginId(SignedUpLoginId))
		{
			if (EditableTextBox_LoginId)
			{
				EditableTextBox_LoginId->SetText(FText::FromString(SignedUpLoginId));
			}
			ShowMessage(LOCTEXT("SignupDone", "가입이 완료되었습니다. 로그인해 주세요."), false);
		}
	}

	SetBusy(Auth && Auth->IsRequestInFlight());
}

void UWarriorLoginWidget::NativeDestruct()
{
	if (Button_Login)
	{
		Button_Login->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoginClicked);
	}

	if (Button_Signup)
	{
		Button_Signup->OnClicked.RemoveDynamic(this, &ThisClass::HandleSignupClicked);
	}

	if (Button_Back)
	{
		Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (EditableTextBox_Password)
	{
		EditableTextBox_Password->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandlePasswordCommitted);
	}

	if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &ThisClass::HandleLoginCompleted);
	}

	//화면을 떠나면 잠김 타이머를 멈춘다(다시 들어와서 시도하면 서버가 다시 429로 알려 준다)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoginLockTimer);
	}
	bLoginLocked = false;

	Super::NativeDestruct();
}

UWidget* UWarriorLoginWidget::GetInitialFocusWidget() const
{
	//아이디가 이미 채워져 있으면(가입 직후) 비밀번호 칸부터
	if (EditableTextBox_LoginId && !EditableTextBox_LoginId->GetText().IsEmpty() && EditableTextBox_Password)
	{
		return EditableTextBox_Password;
	}

	return EditableTextBox_LoginId;
}

void UWarriorLoginWidget::SetBusy(bool bInBusy)
{
	bBusy = bInBusy;

	for (UWidget* Widget : TArray<UWidget*>{ Button_Signup, Button_Back, EditableTextBox_LoginId, EditableTextBox_Password })
	{
		if (Widget)
		{
			Widget->SetIsEnabled(!bInBusy);
		}
	}

	//로그인 버튼은 잠김(429) 중에도 막아 둔다
	if (Button_Login)
	{
		Button_Login->SetIsEnabled(!bInBusy && !bLoginLocked);
	}
}

void UWarriorLoginWidget::LockLoginButton(int32 InSeconds)
{
	UWorld* World = GetWorld();
	if (InSeconds <= 0 || !World)
	{
		return;
	}

	bLoginLocked = true;
	SetBusy(bBusy);
	World->GetTimerManager().SetTimer(LoginLockTimer, this, &ThisClass::HandleLoginLockExpired, static_cast<float>(InSeconds), false);
}

void UWarriorLoginWidget::HandleLoginLockExpired()
{
	bLoginLocked = false;
	SetBusy(bBusy);
	ShowMessage(LOCTEXT("LoginUnlocked", "다시 로그인할 수 있습니다."), false);
}

void UWarriorLoginWidget::SubmitLogin()
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	//잠김 중에는 Enter로도 보내지 않는다
	if (!Auth || Auth->IsRequestInFlight() || bLoginLocked)
	{
		return;
	}

	const FString LoginId = EditableTextBox_LoginId ? EditableTextBox_LoginId->GetText().ToString().TrimStartAndEnd() : FString();
	const FString Password = EditableTextBox_Password ? EditableTextBox_Password->GetText().ToString() : FString();

	//로그인은 필수 입력만 확인한다. 길이 규칙을 여기서 걸면 "그런 아이디가 있는지"를 따로 알려 주는 셈이 된다
	if (LoginId.IsEmpty() || Password.IsEmpty())
	{
		ShowMessage(LOCTEXT("LoginRequired", "아이디와 비밀번호를 입력해 주세요."), true);
		return;
	}

	ShowMessage(LOCTEXT("LoggingIn", "로그인 중..."), false);
	SetBusy(true);
	Auth->RequestLogin(LoginId, Password);
}

void UWarriorLoginWidget::HandleLoginClicked()
{
	SubmitLogin();
}

void UWarriorLoginWidget::HandleSignupClicked()
{
	RequestShowScreen(EWarriorFrontScreen::Signup);
}

void UWarriorLoginWidget::HandleBackClicked()
{
	RequestShowScreen(EWarriorFrontScreen::Title);
}

void UWarriorLoginWidget::HandlePasswordCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	if (InCommitMethod == ETextCommit::OnEnter)
	{
		SubmitLogin();
	}
}

void UWarriorLoginWidget::HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	SetBusy(false);

	if (!bSuccess)
	{
		ShowMessage(Message.IsEmpty() ? LOCTEXT("LoginFailed", "로그인하지 못했습니다. 잠시 후 다시 시도해 주세요.") : Message, true);

		//429 잠김이면 남은 초 동안 로그인 버튼을 막는다
		if (ErrorCode == TEXT("AUTH_LOGIN_LOCKED"))
		{
			if (const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
			{
				LockLoginButton(Auth->GetLoginRetryAfterSeconds());
			}
		}
		return;
	}

	ClearMessage();
	if (EditableTextBox_Password)
	{
		EditableTextBox_Password->SetText(FText::GetEmpty());
	}
	RequestShowScreen(EWarriorFrontScreen::MainMenu);
}

#undef LOCTEXT_NAMESPACE
