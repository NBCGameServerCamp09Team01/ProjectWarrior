// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "ProjectWarrior/Network/WarriorGuildTypes.h"
#include "WarriorGuildWidgets.generated.h"

class UButton;
class UEditableTextBox;
class UImage;
class UListView;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;
class UWarriorGuildPopupWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorGuildPopupClosed);

/**
 * 길드 화면(M23). 도안: docs_local/part-d/ui/guild-ui-draft-1016.html(1.1판, Q1~Q6 확정), 명세: 루트 docs/contracts/guild-api.md v1
 * - 메인메뉴 [길드] → UWarriorGuildPopupWidget. 탭은 소속으로 정한다(미소속: 찾기·내 신청 / 길드원: 내 길드 / 길드장: 내 길드·가입 신청)
 * - 값과 요청은 UWarriorGuildSubsystem(U2). 화면은 G4(내 길드)로 다시 그리고, 동작 결과는 OnGuildActionCompleted로 받는다
 * 버튼·글자는 BindWidgetOptional이라 WBP에 같은 이름으로 두면 붙는다. 뼈대는 WBP_RankingPopup과 같다(Header·Controls·Body Overlay·Footer).
 * 색·여백·모양은 WBP가 맡는다.
 */

//팝업 탭. Switcher_Tabs의 자식 순서와 같다(찾기 → 내 신청 → 내 길드 → 가입 신청)
UENUM(BlueprintType)
enum class EWarriorGuildTab : uint8
{
	Find,
	MyRequests,
	Members,
	JoinRequests
};

//ListView 항목 종류
UENUM(BlueprintType)
enum class EWarriorGuildListItemKind : uint8
{
	Guild,				// 길드 찾기 줄
	Member,				// 내 길드의 길드원 줄
	MyRequest,			// 내 신청 줄(취소)
	ReceivedRequest		// 받은 신청 줄(수락·거절)
};

//ListView 항목 하나. 줄 위젯은 보이는 만큼만 만들어 재사용되고, 값은 이 객체에 남는다(랭킹 항목과 같은 방식)
UCLASS()
class PROJECTWARRIOR_API UWarriorGuildListItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	EWarriorGuildListItemKind Kind = EWarriorGuildListItemKind::Guild;

	UPROPERTY()
	FWarriorGuildSummary Guild;

	UPROPERTY()
	FWarriorGuildMember Member;

	UPROPERTY()
	FWarriorGuildJoinRequest JoinRequest;

	//길드 줄: 이 길드에 이미 대기 중 신청이 있다(버튼 "신청 중")
	UPROPERTY()
	bool bApplied = false;

	//길드 줄: 내 신청이 상한(3곳)이다 / 받은 신청 줄: 내 길드 정원이 찼다. 버튼을 막는다
	UPROPERTY()
	bool bBlocked = false;

	//동작 버튼을 눌러도 되나(다른 요청을 기다리는 중이면 false)
	UPROPERTY()
	bool bActionsEnabled = true;

	//버튼을 누르면 이 팝업이 요청을 보낸다(요청은 팝업 한 곳에서)
	TWeakObjectPtr<UWarriorGuildPopupWidget> Owner;
};

/** 길드 줄 공통: 항목을 받아 그리고, 버튼을 막는 모양을 맞춘다 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGuildRowWidgetBase : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	//~ Begin IUserObjectListEntry Interface.
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~ End IUserObjectListEntry Interface

	//Item이 바뀌었을 때 글자·버튼을 다시 그린다
	virtual void Refresh() {}

	//버튼을 켜거나 막는다. 막힌 동안은 채도를 빼고 어둡게(랭킹 새로 고침과 같은 모양)
	void SetButtonEnabled(UButton* InButton, bool bInEnabled) const;

	//줄의 동작 버튼을 눌렀다. 팝업으로 넘긴다
	void RequestAction(EWarriorGuildAction InAction) const;

	//서버 시각(UTC)을 이 PC 시간대 "10/16 13:02"로
	static FText FormatLocalTime(const FDateTime& InUtc, bool bInWithTime);

	UPROPERTY(Transient)
	TObjectPtr<UWarriorGuildListItem> Item;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FLinearColor InactiveButtonColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.f);
};

/** 길드 찾기 줄: 이름 · 길드장 · 인원 · [가입 신청 / 정원 참 / 신청 중] */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGuildRowWidget : public UWarriorGuildRowWidgetBase
{
	GENERATED_BODY()

public:
	UWarriorGuildRowWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void Refresh() override;

	UFUNCTION()
	void HandleApplyClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Leader;

	//"12/30"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Count;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Apply;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Apply;

	//{0} 지금 인원, {1} 정원
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText CountFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText ApplyText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText FullText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText AppliedText;
};

/** 길드원 줄: 접속 점 · 닉네임 · 길드장 표 · 접속 상태 · 들어온 날. [내보내기]는 Lv2(L3)에서 켠다 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGuildMemberRowWidget : public UWarriorGuildRowWidgetBase
{
	GENERATED_BODY()

public:
	UWarriorGuildMemberRowWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void Refresh() override;

	//접속 점. 색은 C++이 OnlineColor·OfflineColor로 바꾼다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Online;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Nickname;

	//길드장일 때만 보인다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Text_LeaderTag;

	//"접속 중" / "오프라인"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Online;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JoinedAt;

	//Lv2(L3 내보내기) 자리. 지금은 늘 접는다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Kick;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText OnlineText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText OfflineText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FLinearColor OnlineColor = FLinearColor(0.117f, 0.319f, 0.144f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FLinearColor OfflineColor = FLinearColor(0.38f, 0.32f, 0.29f, 1.f);
};

/** 신청 줄: 받은 신청(닉네임 + [수락][거절])과 내 신청(길드 이름 + [신청 취소])이 같이 쓴다 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGuildJoinRequestRowWidget : public UWarriorGuildRowWidgetBase
{
	GENERATED_BODY()

public:
	UWarriorGuildJoinRequestRowWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void Refresh() override;

	UFUNCTION()
	void HandleApproveClicked();

	UFUNCTION()
	void HandleRejectClicked();

	UFUNCTION()
	void HandleCancelClicked();

	//받은 신청: 신청한 사람 닉네임 / 내 신청: 길드 이름
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RequestedAt;

	//"2일 남음"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ExpiresIn;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Approve;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Reject;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cancel;

	//{0} 남은 일 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText DaysLeftFormat;

	//{0} 남은 시간(하루가 안 남았을 때)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText HoursLeftFormat;
};

/** 길드 만들기 대화상자(팝업 위 한 겹). 입력할 때마다 이름 규칙 셋을 게임에서 먼저 보고, 다 맞아야 [만들기] */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGuildCreateDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorGuildCreateDialogWidget(const FObjectInitializer& ObjectInitializer);

	//빈 입력으로 보인다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void Close();

	bool IsOpen() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//규칙 결과가 바뀔 때(✓·✗ 색은 C++이 넣고, 그 밖의 모양은 BP에서)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Guild", meta = (DisplayName = "On Name Checked"))
	void BP_OnNameChecked(bool bInLengthOk, bool bInCharsOk, bool bInSpacesOk);

	UFUNCTION()
	void HandleNameChanged(const FText& InText);

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleActionCompleted(EWarriorGuildAction InAction, bool bInSuccess, const FString& InErrorCode, const FText& InMessage);

	void UpdateRules();
	void SetNameError(const FText& InMessage);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditableTextBox_Name;

	//"7/12"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NameCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RuleLength;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RuleChars;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RuleSpaces;

	//서버가 거절한 이유(이름 중복·상한 등)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NameError;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Create;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cancel;

	//{0} 지금 글자 수, {1} 최대
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText CountFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText RuleLengthText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText RuleCharsText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText RuleSpacesText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FSlateColor RuleOkColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FSlateColor RuleNgColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FLinearColor InactiveButtonColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.f);

private:
	//이 대화상자가 보낸 만들기 요청을 기다리는 중
	bool bWaitingCreate = false;
};

/**
 * 메인메뉴 길드 팝업. 열 때 G4를 다시 받고, 길드 서브시스템 알림마다 지금 탭을 다시 그린다.
 * - 쓰는 중(IsActionInFlight)엔 동작 버튼을 막는다. 동작 실패는 문구 표의 방법대로: Popup → 공용 팝업, 그 밖 → Text_ActionError
 * - 탈퇴·해체만 확인 칸(Widget_Confirm)으로 한 번 더 묻는다(Q3). 거절·신청 취소는 바로 보낸다
 * - 화면이 떠 있는 동안만 알림을 받는다(랭킹 팝업과 같음)
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorGuildPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorGuildPopupWidget(const FObjectInitializer& ObjectInitializer);

	//팝업을 열 때마다. 소속의 첫 탭부터 보이고 G4를 다시 받는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void Open();

	//팝업을 닫고 OnClosed로 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void Close();

	//줄의 버튼을 눌렀다(UWarriorGuildRowWidgetBase가 부른다)
	void HandleRowAction(const UWarriorGuildListItem* InItem, EWarriorGuildAction InAction);

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorGuildPopupClosed OnClosed;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//탭이 정해질 때. 선택 테두리는 BP에서
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Guild", meta = (DisplayName = "On Tab Set"))
	void BP_OnTabSet(EWarriorGuildTab InTab);

	//불러오는 중·실패 상태가 바뀔 때. 패널 보이기는 C++이 하고, 그 밖의 모양은 BP에서
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Guild", meta = (DisplayName = "On View State Set"))
	void BP_OnViewStateSet(bool bInLoading, bool bInFailed);

	//~ Begin 길드 서브시스템 알림
	UFUNCTION()
	void HandleMyGuildUpdated();

	UFUNCTION()
	void HandleMyGuildChanged(const FString& InOldGuildId, const FString& InNewGuildId);

	UFUNCTION()
	void HandleGuildListUpdated();

	UFUNCTION()
	void HandleJoinRequestsUpdated();

	UFUNCTION()
	void HandleActionCompleted(EWarriorGuildAction InAction, bool bInSuccess, const FString& InErrorCode, const FText& InMessage);

	UFUNCTION()
	void HandleRequestFailed(const FString& InErrorCode, const FText& InMessage);
	//~ End 길드 서브시스템 알림

	//~ Begin 버튼
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleTabFindClicked();

	UFUNCTION()
	void HandleTabMyRequestsClicked();

	UFUNCTION()
	void HandleTabMembersClicked();

	UFUNCTION()
	void HandleTabJoinRequestsClicked();

	UFUNCTION()
	void HandleSearchClicked();

	UFUNCTION()
	void HandleSearchCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UFUNCTION()
	void HandleDisbandClicked();

	UFUNCTION()
	void HandleConfirmYesClicked();

	UFUNCTION()
	void HandleConfirmNoClicked();
	//~ End 버튼

	//소속에서 이 탭을 쓸 수 있나
	bool IsTabAllowed(EWarriorGuildTab InTab) const;

	//소속의 첫 탭
	EWarriorGuildTab GetFirstTab() const;

	//탭 버튼(없으면 nullptr)
	UButton* GetTabButton(EWarriorGuildTab InTab) const;

	//탭을 고른다(쓸 수 없는 탭이면 첫 탭). 그 탭에 필요한 목록을 처음이면 받는다
	void SelectTab(EWarriorGuildTab InTab);

	//제목·탭·목록·아래 버튼을 지금 값으로 다시 그린다
	void Render();
	void RenderTabs();
	void RenderList();
	void RenderFooter();
	void FillList(UListView* InListView, const TArray<UWarriorGuildListItem*>& InItems);

	void SendSearch();
	void ShowConfirm(EWarriorGuildAction InAction);
	void HideConfirm();
	void SetActionError(const FText& InMessage);
	void SetButtonEnabled(UButton* InButton, bool bInEnabled) const;

	//길드 목록 스크롤. 끝에 가까우면 다음 묶음(랭킹 팝업과 같은 방식)
	void HandleGuildListScrolled(float InItemOffset, float InDistanceRemaining);

	//~ Begin 없어도 되는 위젯(WBP에 같은 이름으로 두면 연결된다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	//"3/30"(소속이 있을 때만)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MemberCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabFind;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabMyRequests;

	//"2/3"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MyRequestCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabMembers;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabJoinRequests;

	//받은 신청 수. 0이면 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JoinRequestBadge;

	//검색 줄(찾기 탭에서만 보임). 안에 EditableTextBox_Search·Button_Search
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_SearchRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditableTextBox_Search;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Search;

	//탭 내용. 자식 순서 = EWarriorGuildTab
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> Switcher_Tabs;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ListView_Guilds;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ListView_MyRequests;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ListView_Members;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ListView_JoinRequests;

	//불러오는 중 표시(G4를 처음 받기 전)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Loading;

	//불러오기 실패(안에 Text_Error·Button_Retry). 탭 내용 자리를 대신한다(랭킹과 같은 이름)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Error;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Error;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Retry;

	//지금 탭이 비었을 때 문구
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Empty;

	//동작 실패 한 줄(문구 표의 Inline·Field)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ActionError;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Create;

	//만들 수 없을 때 이유(조건 미충족)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CreateHint;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Leave;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Disband;

	//탈퇴·해체 확인 칸(팝업 위 한 겹). 안에 Text_Confirm·Button_ConfirmYes·Button_ConfirmNo
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Confirm;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Confirm;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ConfirmYes;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ConfirmNo;

	//만들기 대화상자(팝업 위 한 겹, WBP_GuildCreateDialog)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWarriorGuildCreateDialogWidget> CreateDialog;
	//~ End 없어도 되는 위젯

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText NoGuildTitle;

	//{0} 지금 인원, {1} 정원
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText MemberCountFormat;

	//{0} 내 대기 중 신청 수, {1} 상한
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText MyRequestCountFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText LoadFailedText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText EmptyFindText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText EmptySearchText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText EmptyMyRequestsText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText EmptyJoinRequestsText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText SearchInvalidText;

	//만들기 조건 미충족 안내. {0} 클리어해야 하는 스테이지 키
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText CreateHintFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText CreateHintJoinedText;

	//{0} 길드 이름
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText LeaveConfirmFormat;

	//길드장이 혼자일 때 [탈퇴]. {0} 길드 이름
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText LeaveAloneConfirmFormat;

	//{0} 길드 이름, {1} 길드원 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Text")
	FText DisbandConfirmFormat;

	//한 사람의 대기 중 신청 상한(명세 규칙 3곳). 내 신청 탭 표시·신청 버튼 막기에 쓴다(서버가 최종 판정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild", meta = (ClampMin = "1"))
	int32 MaxPendingRequests = 3;

	//남은 스크롤이 이 비율(0~1, 0 = 맨 아래) 아래로 내려가면 다음 묶음을 받는다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Scroll", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LoadMoreThreshold = 0.15f;

	//다음 묶음 요청 사이 최소 간격(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Scroll", meta = (ClampMin = "0.0"))
	float LoadMoreMinInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild|Look")
	FLinearColor InactiveButtonColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.f);

private:
	EWarriorGuildTab CurrentTab = EWarriorGuildTab::Find;

	//G4를 받지 못했다(처음 불러오기 실패)
	bool bLoadFailed = false;

	//이번에 열린 뒤 길드 목록·받은 신청을 요청했나(탭을 처음 열 때 한 번)
	bool bGuildListRequested = false;
	bool bJoinRequestsRequested = false;

	//지금 목록의 검색어(빈 값 = 전체)
	FString SearchPrefix;

	//확인 칸이 묻고 있는 동작(Leave·Disband). 묻지 않으면 None 대신 bConfirming false
	bool bConfirming = false;
	EWarriorGuildAction ConfirmAction = EWarriorGuildAction::Leave;

	double LastLoadMoreSeconds = 0.0;

	//탭 버튼 넷의 WBP 스타일(EWarriorGuildTab 순서). 고른 탭 강조 뒤 되돌릴 때 쓴다
	UPROPERTY(Transient)
	TArray<FButtonStyle> TabDefaultStyles;
};
