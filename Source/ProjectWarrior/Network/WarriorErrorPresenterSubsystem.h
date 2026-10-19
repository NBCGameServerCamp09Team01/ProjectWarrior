// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorErrorPresenterSubsystem.generated.h"

class UWarriorNoticePopupWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorErrorPopupClosed);

/**
 * 오류 팝업 담당(S5). 레벨(프론트·스테이지)과 상관없이 지금 플레이어 화면에 공용 팝업을 띄운다.
 * - UWarriorApiSubsystem::OnNoticeRequested를 받아 하나씩 띄운다(떠 있는 동안 온 것은 차례를 기다린다).
 * - "다시 시도 / 게임 종료" 팝업의 답은 UWarriorApiSubsystem::AnswerRetryPrompt로 돌려준다.
 * - Action이 Quit인 안내는 확인을 누르면 게임을 끝낸다.
 * - 팝업 위젯 클래스는 UWarriorApiSettings::ErrorPopupClass(WBP_NoticePopup).
 * 세션 종료 안내(401·다른 곳 로그인)는 지금처럼 프론트 컨트롤러가 띄운다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorErrorPresenterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorErrorPresenterSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//팝업이 떠 있는가(차례를 기다리는 것 포함)
	UFUNCTION(BlueprintPure, Category = "Warrior|Api")
	bool IsShowing() const { return bShowing; }

	//팝업이 닫혔을 때. 프론트 컨트롤러가 아래 화면으로 포커스를 돌려준다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Api")
	FOnWarriorErrorPopupClosed OnPopupClosed;

private:
	UFUNCTION()
	void HandleNoticeRequested(const FWarriorErrorNotice& InNotice);

	UFUNCTION()
	void HandlePopupChoice(bool bConfirmed);

	//레벨을 옮기면 뷰포트의 위젯이 사라지므로, 떠 있던 팝업을 새 레벨에 다시 띄운다
	void HandlePostLoadMap(UWorld* InWorld);

	//CurrentNotice를 띄운다. 띄울 수 없으면(위젯 클래스 없음 등) 버튼을 누른 것처럼 처리한다
	void ShowCurrent();

	//다음 차례를 띄운다. 없으면 닫힘을 알린다
	void ShowNext();

	//버튼 답을 처리한다(다시 시도 답·종료)
	void ApplyChoice(const FWarriorErrorNotice& InNotice, bool bConfirmed);

	void QuitGame();

	TArray<FWarriorErrorNotice> Queue;

	FWarriorErrorNotice CurrentNotice;

	bool bShowing = false;

	//팝업을 띄우기 전 마우스 커서 표시. 게임 중(커서 없음)에 띄웠다면 닫을 때 게임 입력으로 되돌린다
	bool bCursorBeforePopup = true;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorNoticePopupWidget> Popup;

	FDelegateHandle PostLoadMapHandle;
};
