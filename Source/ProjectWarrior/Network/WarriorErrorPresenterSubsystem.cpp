// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorErrorPresenterSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Network/WarriorApiSettings.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Widgets/WarriorNoticePopupWidget.h"

#define LOCTEXT_NAMESPACE "WarriorErrorPresenter"

namespace WarriorErrorPresenter
{
	//화면·스테이지 선택·세션 종료 안내 팝업(50)보다 위
	const int32 PopupZOrder = 100;
}

UWarriorErrorPresenterSubsystem* UWarriorErrorPresenterSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorErrorPresenterSubsystem>() : nullptr;
}

void UWarriorErrorPresenterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWarriorApiSubsystem* Api = Collection.InitializeDependency<UWarriorApiSubsystem>())
	{
		Api->OnNoticeRequested.AddUniqueDynamic(this, &ThisClass::HandleNoticeRequested);
	}

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UWarriorErrorPresenterSubsystem::Deinitialize()
{
	if (UWarriorApiSubsystem* Api = GetGameInstance()->GetSubsystem<UWarriorApiSubsystem>())
	{
		Api->OnNoticeRequested.RemoveDynamic(this, &ThisClass::HandleNoticeRequested);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	Super::Deinitialize();
}

void UWarriorErrorPresenterSubsystem::HandleNoticeRequested(const FWarriorErrorNotice& InNotice)
{
	if (bShowing)
	{
		Queue.Add(InNotice);
		return;
	}

	CurrentNotice = InNotice;
	bShowing = true;
	ShowCurrent();
}

void UWarriorErrorPresenterSubsystem::ShowCurrent()
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[ErrorPopup] Show. code=%s action=%s ask=%s"),
		*CurrentNotice.Code, *UEnum::GetValueAsString(CurrentNotice.Action), CurrentNotice.bAskRetry ? TEXT("true") : TEXT("false"));

	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	const TSubclassOf<UWarriorNoticePopupWidget> PopupClass = UWarriorApiSettings::Get()->ErrorPopupClass.LoadSynchronous();
	if (!PlayerController || !PopupClass)
	{
		//띄울 곳이 없다. 묻는 팝업이면 종료를 고른 것으로(보관은 이미 했다), 종료 안내면 그대로 종료한다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[ErrorPopup] Cannot show popup (player controller=%s, ErrorPopupClass=%s). Message: %s"),
			PlayerController ? TEXT("ok") : TEXT("none"), PopupClass ? TEXT("ok") : TEXT("none"), *CurrentNotice.Message.ToString());
		const FWarriorErrorNotice Notice = CurrentNotice;
		ApplyChoice(Notice, false);
		ShowNext();
		return;
	}

	if (Popup)
	{
		Popup->OnChoice.RemoveDynamic(this, &ThisClass::HandlePopupChoice);
		Popup->RemoveFromParent();
	}

	Popup = CreateWidget<UWarriorNoticePopupWidget>(PlayerController, PopupClass);
	if (!Popup)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[ErrorPopup] Failed to create popup."));
		const FWarriorErrorNotice Notice = CurrentNotice;
		ApplyChoice(Notice, false);
		ShowNext();
		return;
	}

	FText Message = CurrentNotice.Message;
	if (CurrentNotice.bAskRetry)
	{
		if (Popup->HasSecondaryButton())
		{
			Popup->SetButtons(LOCTEXT("Retry", "다시 시도"), LOCTEXT("QuitGame", "게임 종료"));
		}
		else
		{
			//두 번째 버튼이 아직 WBP에 없다: 확인 = 다시 시도(다시 실패하면 종료 안내가 뜬다)
			Popup->SetButtons(LOCTEXT("Retry", "다시 시도"), FText::GetEmpty());
			Message = FText::Format(LOCTEXT("RetryOnly", "{0}\n\n다시 시도를 누르면 한 번 더 보냅니다."), Message);
		}
	}
	else
	{
		Popup->SetButtons(LOCTEXT("Confirm", "확인"), FText::GetEmpty());
	}

	Popup->SetMessage(Message);
	Popup->OnChoice.AddUniqueDynamic(this, &ThisClass::HandlePopupChoice);
	Popup->AddToViewport(WarriorErrorPresenter::PopupZOrder);

	bCursorBeforePopup = PlayerController->ShouldShowMouseCursor();
	PlayerController->SetShowMouseCursor(true);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, Popup);
}

void UWarriorErrorPresenterSubsystem::HandlePopupChoice(bool bConfirmed)
{
	if (Popup)
	{
		Popup->OnChoice.RemoveDynamic(this, &ThisClass::HandlePopupChoice);
	}

	//게임 중에 띄웠다면 게임 입력으로 되돌린다
	if (APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController())
	{
		if (!bCursorBeforePopup)
		{
			PlayerController->SetShowMouseCursor(false);
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController);
		}
	}

	const FWarriorErrorNotice Notice = CurrentNotice;
	ApplyChoice(Notice, bConfirmed);
	ShowNext();
}

void UWarriorErrorPresenterSubsystem::ApplyChoice(const FWarriorErrorNotice& InNotice, bool bConfirmed)
{
	if (InNotice.bAskRetry)
	{
		//확인 = 다시 시도, 두 번째 버튼 = 게임 종료
		if (UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this))
		{
			Api->AnswerRetryPrompt(bConfirmed);
		}
		return;
	}

	if (InNotice.Action == EWarriorErrorAction::Quit)
	{
		QuitGame();
	}
}

void UWarriorErrorPresenterSubsystem::ShowNext()
{
	if (Queue.IsEmpty())
	{
		bShowing = false;
		Popup = nullptr;
		OnPopupClosed.Broadcast();
		return;
	}

	CurrentNotice = Queue[0];
	Queue.RemoveAt(0);
	ShowCurrent();
}

void UWarriorErrorPresenterSubsystem::HandlePostLoadMap(UWorld* InWorld)
{
	//이전 레벨의 위젯은 뷰포트와 함께 사라졌다. 답을 받지 못한 팝업을 새 레벨에 다시 띄운다
	if (bShowing && InWorld && InWorld->GetGameInstance() == GetGameInstance())
	{
		Popup = nullptr;
		ShowCurrent();
	}
}

void UWarriorErrorPresenterSubsystem::QuitGame()
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[ErrorPopup] Quitting the game."));
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	UKismetSystemLibrary::QuitGame(GetGameInstance(), PlayerController, EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE
