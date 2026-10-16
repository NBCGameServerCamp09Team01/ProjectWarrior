// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWarrior/Chat/WarriorChatTypes.h"
#include "WarriorChatWidgets.generated.h"

class UButton;
class UEditableTextBox;
class UListView;
class UTextBlock;
class UWidget;

/**
 * 메인메뉴 채팅 창(M23 U-C2). 명세: 루트 docs/contracts/chat-api.md "게임 처리 규칙", 설계판 C-5·C-6·C-7
 * - 메인메뉴의 Slot_Chat에 UWarriorChatSettings::ChatPanelClass(WBP_ChatPanel, 부모 = UWarriorChatPanelWidget)가 들어간다(U-C0).
 * - 줄·상태는 UWarriorChatSubsystem이 들고, 이 창은 그리기만 한다(창을 닫았다 열어도 줄이 남음).
 * - 버튼·글자는 BindWidgetOptional이라 WBP에 같은 이름으로 두면 붙는다. 색·여백·모양은 WBP가 맡는다.
 */

//ListView 항목 하나(날짜선 또는 채팅 줄). 다시 그릴 때마다 새로 만든다(탭당 최대 50줄 + 날짜선)
UCLASS()
class PROJECTWARRIOR_API UWarriorChatRowItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bDateLine = false;

	//날짜선이면 그 날짜(PC 시간대)
	UPROPERTY()
	FDateTime LocalDate;

	//채팅 줄이면 그 줄
	UPROPERTY()
	FWarriorChatLine Line;

	UPROPERTY()
	EWarriorChatScope Scope = EWarriorChatScope::Global;

	//내가 보낸 줄인가(로그인한 계정)
	UPROPERTY()
	bool bMine = false;
};

//채팅 창 한 줄(ListView 항목 줄). 날짜선이면 Panel_Date만, 채팅 줄이면 Panel_Line만 보인다
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorChatLineWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UWarriorChatLineWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin IUserObjectListEntry Interface.
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~ End IUserObjectListEntry Interface

protected:
	virtual void NativeOnInitialized() override;

	//줄이 정해질 때. 내 줄 색·보내는 중 흐리게 등 모양은 BP에서
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Chat", meta = (DisplayName = "On Line Set"))
	void BP_OnLineSet(bool bInDateLine, bool bInMine, EWarriorChatLineState InState);

	UFUNCTION()
	void HandleRetryClicked();

	//채팅 줄 칸(시각·닉네임·글·상태)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Line;

	//날짜선 칸
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Date;

	//HH:MM(PC 시간대)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Time;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Nickname;

	//글(서식 없이 그대로)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Message;

	//"보내는 중" / "보내지 못함". 보낸 줄이면 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_State;

	//보내지 못한 줄만 보임. 같은 requestId로 다시 보낸다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Retry;

	//날짜선 글자("10월 16일 (금)")
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Date;

	//{0} 월, {1} 일, {2} 요일 한 글자
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText DateLineFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText SendingText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText FailedText;

private:
	UPROPERTY()
	TObjectPtr<UWarriorChatRowItem> Item;
};

/**
 * 채팅 창: [전체][길드] 탭, 줄 목록, 입력 칸, 보내기.
 * - 맨 아래를 보고 있을 때만 새 줄을 따라 내려간다(위로 올려 읽는 중이면 그대로). 내가 보낸 줄은 늘 맨 아래로.
 * - 길드 탭에서 맨 위까지 올리면 이전 묶음(CH3 before)을 받아 위에 붙이고, 보던 줄 위치를 지킨다.
 * - Enter = 보내기(입력 칸에 포커스가 있을 때). 입력은 코드 포인트 GetMaxTextLength()자까지만.
 * - 429 동안 보내기 버튼을 막고 남은 초를 보인다. 서버가 받지 않은 글(400·403·429)은 입력 칸에 되돌린다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorChatPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorChatPanelWidget(const FObjectInitializer& ObjectInitializer);

	//탭을 바꾼다. 길드 탭을 처음 열면 길드 내역 첫 묶음을 받는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	void SelectTab(EWarriorChatScope InScope);

	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	EWarriorChatScope GetSelectedTab() const { return SelectedScope; }

	//입력 칸의 글을 지금 탭으로 보낸다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	void SendInput();

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	//탭이 바뀔 때(고른 탭 버튼 모양은 BP에서)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Chat", meta = (DisplayName = "On Tab Selected"))
	void BP_OnTabSelected(EWarriorChatScope InScope);

	//보내기 막힘이 바뀔 때(버튼 흐리게 등은 BP에서. 글자·활성은 C++이 넣는다)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Chat", meta = (DisplayName = "On Send Blocked Changed"))
	void BP_OnSendBlockedChanged(bool bInBlocked);

	UFUNCTION()
	void HandleTabGlobalClicked();

	UFUNCTION()
	void HandleTabGuildClicked();

	UFUNCTION()
	void HandleSendClicked();

	UFUNCTION()
	void HandleInputChanged(const FText& InText);

	UFUNCTION()
	void HandleInputCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	UFUNCTION()
	void HandleChatUpdated(EWarriorChatScope InScope);

	UFUNCTION()
	void HandleSendStateChanged(bool bInBlocked, const FText& InNotice);

	UFUNCTION()
	void HandleSendRejected(EWarriorChatScope InScope, const FString& InCode, const FText& InMessage, const FString& InText);

	UFUNCTION()
	void HandleGuildStateChanged(EWarriorChatGuildState InState);

	void HandleListScrolled(float InItemOffset, float InDistanceRemaining);

	//지금 탭의 줄을 다시 그린다. 따라 내려가기·위에 붙인 뒤 위치 지키기를 여기서 한다
	void Rebuild(bool bInForceBottom);

	//빈 탭 안내·보내기 버튼 상태
	void UpdateEmptyText();
	void UpdateSendButton();
	void SetNotice(const FText& InNotice);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabGlobal;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabGuild;

	//항목 줄 = UWarriorChatLineWidget을 부모로 둔 WBP
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ListView_Lines;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditableTextBox_Input;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Send;

	//보내기 버튼 글자("보내기" / "보내기 (7)")
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Send;

	//창 안 안내(429 남은 초·글 확인·길드 없음 등). 비면 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Notice;

	//줄이 없을 때 목록 자리 문구(길드 없음 / 아직 채팅 없음)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText SendText;

	//{0} 남은 초
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText SendBlockedFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText EmptyText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText GuildNoneText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Text")
	FText TextInvalidText;

	//맨 아래로 보는 남은 스크롤(0~1, 0 = 맨 아래). 이 아래면 새 줄을 따라 내려간다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Scroll", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FollowBottomThreshold = 0.02f;

	//맨 위로 보는 줄 위치(항목 수). 이 위로 올리면 길드 이전 묶음을 받는다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Chat|Scroll", meta = (ClampMin = "0.0"))
	float LoadOlderOffset = 0.5f;

private:
	EWarriorChatScope SelectedScope = EWarriorChatScope::Global;

	//마지막 스크롤 때 맨 아래였나(처음은 맨 아래)
	bool bAtBottom = true;

	//보내기 막힘 남은 초(버튼 글자를 바꿀 때만 다시 씀)
	int32 ShownBlockedSeconds = -1;

	//글자 수를 자르느라 입력 칸 글을 바꾸는 중(바뀜 알림이 다시 오지 않게)
	bool bTrimmingInput = false;
};
