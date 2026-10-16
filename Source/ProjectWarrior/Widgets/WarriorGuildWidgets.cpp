// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGuildWidgets.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "HAL/PlatformTime.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorGuildName.h"
#include "ProjectWarrior/Network/WarriorGuildSubsystem.h"

#define LOCTEXT_NAMESPACE "WarriorGuild"

namespace WarriorGuildView
{
	void SetText(UTextBlock* InText, const FText& InValue)
	{
		if (InText)
		{
			InText->SetText(InValue);
		}
	}

	//패널·글자: 보이되 자기는 클릭을 받지 않는다(안의 버튼은 받음)
	void ShowPanel(UWidget* InWidget, bool bInShown)
	{
		if (InWidget)
		{
			InWidget->SetVisibility(bInShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	//버튼: 보이면 클릭을 받아야 하므로 Visible
	void ShowButton(UWidget* InWidget, bool bInShown)
	{
		if (InWidget)
		{
			InWidget->SetVisibility(bInShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}

	void SetButtonEnabled(UButton* InButton, bool bInEnabled, const FLinearColor& InInactiveColor)
	{
		if (InButton)
		{
			InButton->SetIsEnabled(bInEnabled);
			//막힌 동안은 채도를 빼고 어둡게(랭킹 도안 2.1판과 같은 모양). 풀리면 원래 스타일
			InButton->SetColorAndOpacity(bInEnabled ? FLinearColor::White : InInactiveColor);
		}
	}

	/**
	 * 동작 실패를 문구 표(맥락 Guild)의 방법대로 보인다. Popup이면 공용 팝업, 그 밖(Inline·Field 등)은 InInlineText에.
	 * 공용 팝업으로 보였으면 true
	 */
	bool PresentActionError(const UObject* InContext, const FString& InCode, const FText& InMessage, UTextBlock* InInlineText)
	{
		FWarriorApiError Error;
		Error.Code = InCode;
		Error.HttpStatus = -1;	//상태 기본값을 타지 않게 한다(코드로만 고름)
		const FWarriorErrorRule Rule = FWarriorErrorText::Resolve(Error, WarriorErrorContext::Guild);
		const FText Message = InMessage.IsEmpty() ? Rule.Text : InMessage;

		if (Rule.Presentation == EWarriorErrorPresentation::Popup)
		{
			if (UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(InContext))
			{
				FWarriorErrorNotice Notice;
				Notice.Code = InCode;
				Notice.Message = Message;
				Api->RequestNotice(Notice);
				return true;
			}
		}
		SetText(InInlineText, Message);
		return false;
	}
}

//──────────────────────────────── 줄 공통

void UWarriorGuildRowWidgetBase::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	//ListView가 줄 위젯을 재사용할 때마다 다른 항목을 넣는다
	Item = Cast<UWarriorGuildListItem>(ListItemObject);
	if (Item)
	{
		Refresh();
	}
}

void UWarriorGuildRowWidgetBase::SetButtonEnabled(UButton* InButton, bool bInEnabled) const
{
	WarriorGuildView::SetButtonEnabled(InButton, bInEnabled, InactiveButtonColor);
}

void UWarriorGuildRowWidgetBase::RequestAction(EWarriorGuildAction InAction) const
{
	if (Item && Item->Owner.IsValid())
	{
		Item->Owner->HandleRowAction(Item, InAction);
	}
}

FText UWarriorGuildRowWidgetBase::FormatLocalTime(const FDateTime& InUtc, bool bInWithTime)
{
	if (InUtc == FDateTime())
	{
		return FText::GetEmpty();
	}
	//서버 시각은 UTC. 이 PC의 시간대로 보인다(랭킹·스테이지 선택과 같은 방식)
	const FDateTime LocalTime = InUtc + (FDateTime::Now() - FDateTime::UtcNow());
	return FText::FromString(LocalTime.ToString(bInWithTime ? TEXT("%m/%d %H:%M") : TEXT("%m/%d")));
}

//──────────────────────────────── 길드 찾기 줄

UWarriorGuildRowWidget::UWarriorGuildRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CountFormat = LOCTEXT("RowCountFormat", "{0}/{1}");
	ApplyText = LOCTEXT("ApplyText", "가입 신청");
	FullText = LOCTEXT("FullText", "정원 참");
	AppliedText = LOCTEXT("AppliedText", "신청 중");
}

void UWarriorGuildRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Apply)
	{
		Button_Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyClicked);
	}
}

void UWarriorGuildRowWidget::Refresh()
{
	const FWarriorGuildSummary& Guild = Item->Guild;
	WarriorGuildView::SetText(Text_Name, FText::FromString(Guild.Name));
	WarriorGuildView::SetText(Text_Leader, FText::FromString(Guild.LeaderNickname));
	WarriorGuildView::SetText(Text_Count, FText::Format(CountFormat, Guild.MemberCount, Guild.MaxMembers));

	//신청 중 → 정원 참 → 가입 신청 순으로 본다. 정원이 찬 길드도 목록에는 나온다(명세 G1, 서버도 GUILD_FULL로 거절)
	const bool bFull = Guild.MaxMembers > 0 && Guild.MemberCount >= Guild.MaxMembers;
	WarriorGuildView::SetText(Text_Apply, Item->bApplied ? AppliedText : bFull ? FullText : ApplyText);
	SetButtonEnabled(Button_Apply, !Item->bApplied && !bFull && !Item->bBlocked && Item->bActionsEnabled);
}

void UWarriorGuildRowWidget::HandleApplyClicked()
{
	RequestAction(EWarriorGuildAction::Apply);
}

//──────────────────────────────── 길드원 줄

UWarriorGuildMemberRowWidget::UWarriorGuildMemberRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OnlineText = LOCTEXT("OnlineText", "접속 중");
	OfflineText = LOCTEXT("OfflineText", "오프라인");
}

void UWarriorGuildMemberRowWidget::Refresh()
{
	const FWarriorGuildMember& Member = Item->Member;
	if (Image_Online)
	{
		Image_Online->SetColorAndOpacity(Member.bOnline ? OnlineColor : OfflineColor);
	}
	WarriorGuildView::SetText(Text_Nickname, FText::FromString(Member.Nickname));
	WarriorGuildView::ShowPanel(Text_LeaderTag, Member.Role == EWarriorGuildRole::Leader);
	WarriorGuildView::SetText(Text_Online, Member.bOnline ? OnlineText : OfflineText);
	WarriorGuildView::SetText(Text_JoinedAt, FormatLocalTime(Member.JoinedAt, false));

	//내보내기는 Lv2(L3)에서 켠다
	WarriorGuildView::ShowButton(Button_Kick, false);
}

//──────────────────────────────── 신청 줄

UWarriorGuildJoinRequestRowWidget::UWarriorGuildJoinRequestRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DaysLeftFormat = LOCTEXT("DaysLeftFormat", "{0}일 남음");
	HoursLeftFormat = LOCTEXT("HoursLeftFormat", "{0}시간 남음");
}

void UWarriorGuildJoinRequestRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Approve)
	{
		Button_Approve->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApproveClicked);
	}
	if (Button_Reject)
	{
		Button_Reject->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRejectClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UWarriorGuildJoinRequestRowWidget::Refresh()
{
	const FWarriorGuildJoinRequest& Request = Item->JoinRequest;
	const bool bReceived = Item->Kind == EWarriorGuildListItemKind::ReceivedRequest;

	WarriorGuildView::SetText(Text_Name, FText::FromString(bReceived ? Request.Nickname : Request.GuildName));
	WarriorGuildView::SetText(Text_RequestedAt, FormatLocalTime(Request.RequestedAt, true));

	//남은 기간: 하루 이상이면 일(내림), 아니면 시간(올림, 최소 1)
	const FTimespan Left = Request.ExpiresAt - FDateTime::UtcNow();
	const int32 Days = FMath::FloorToInt(Left.GetTotalDays());
	WarriorGuildView::SetText(Text_ExpiresIn, Days >= 1
		? FText::Format(DaysLeftFormat, Days)
		: FText::Format(HoursLeftFormat, FMath::Max(1, FMath::CeilToInt(Left.GetTotalHours()))));

	WarriorGuildView::ShowButton(Button_Approve, bReceived);
	WarriorGuildView::ShowButton(Button_Reject, bReceived);
	WarriorGuildView::ShowButton(Button_Cancel, !bReceived);
	//수락은 내 길드 정원이 차면 막는다(서버도 GUILD_FULL로 거절). 거절·취소는 늘 된다
	SetButtonEnabled(Button_Approve, Item->bActionsEnabled && !Item->bBlocked);
	SetButtonEnabled(Button_Reject, Item->bActionsEnabled);
	SetButtonEnabled(Button_Cancel, Item->bActionsEnabled);
}

void UWarriorGuildJoinRequestRowWidget::HandleApproveClicked()
{
	RequestAction(EWarriorGuildAction::Approve);
}

void UWarriorGuildJoinRequestRowWidget::HandleRejectClicked()
{
	RequestAction(EWarriorGuildAction::Reject);
}

void UWarriorGuildJoinRequestRowWidget::HandleCancelClicked()
{
	RequestAction(EWarriorGuildAction::Cancel);
}

//──────────────────────────────── 만들기 대화상자

UWarriorGuildCreateDialogWidget::UWarriorGuildCreateDialogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CountFormat = LOCTEXT("NameCountFormat", "{0}/{1}");
	RuleLengthText = LOCTEXT("RuleLength", "2~12자");
	RuleCharsText = LOCTEXT("RuleChars", "한글·영문·숫자");
	RuleSpacesText = LOCTEXT("RuleSpaces", "공백은 가운데 한 칸만");
	RuleOkColor = FSlateColor(FLinearColor(0.37f, 0.60f, 0.42f, 1.f));
	RuleNgColor = FSlateColor(FLinearColor(0.89f, 0.45f, 0.35f, 1.f));
}

void UWarriorGuildCreateDialogWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EditableTextBox_Name)
	{
		EditableTextBox_Name->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleNameChanged);
	}
	if (Button_Create)
	{
		Button_Create->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UWarriorGuildCreateDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->OnGuildActionCompleted.AddUniqueDynamic(this, &ThisClass::HandleActionCompleted);
	}
}

void UWarriorGuildCreateDialogWidget::NativeDestruct()
{
	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->OnGuildActionCompleted.RemoveDynamic(this, &ThisClass::HandleActionCompleted);
	}

	Super::NativeDestruct();
}

void UWarriorGuildCreateDialogWidget::Open()
{
	bWaitingCreate = false;
	if (EditableTextBox_Name)
	{
		EditableTextBox_Name->SetText(FText::GetEmpty());
	}
	SetNameError(FText::GetEmpty());
	SetVisibility(ESlateVisibility::Visible);
	UpdateRules();

	if (EditableTextBox_Name)
	{
		EditableTextBox_Name->SetKeyboardFocus();
	}
}

void UWarriorGuildCreateDialogWidget::Close()
{
	bWaitingCreate = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UWarriorGuildCreateDialogWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

void UWarriorGuildCreateDialogWidget::HandleNameChanged(const FText& InText)
{
	//입력을 바꾸면 지난 서버 오류(이름 중복 등)는 지운다
	SetNameError(FText::GetEmpty());
	UpdateRules();
}

void UWarriorGuildCreateDialogWidget::UpdateRules()
{
	const FString Name = EditableTextBox_Name ? EditableTextBox_Name->GetText().ToString() : FString();
	const FWarriorGuildNameCheck Check = FWarriorGuildName::Check(Name);

	auto SetRule = [this](UTextBlock* InText, const FText& InLabel, bool bInOk)
	{
		if (InText)
		{
			InText->SetText(FText::Format(LOCTEXT("RuleFormat", "{0} {1}"), FText::FromString(bInOk ? TEXT("✓") : TEXT("✗")), InLabel));
			InText->SetColorAndOpacity(bInOk ? RuleOkColor : RuleNgColor);
		}
	};
	SetRule(Text_RuleLength, RuleLengthText, Check.bLengthOk);
	SetRule(Text_RuleChars, RuleCharsText, Check.bCharsOk);
	SetRule(Text_RuleSpaces, RuleSpacesText, Check.bSpacesOk);
	WarriorGuildView::SetText(Text_NameCount, FText::Format(CountFormat, Name.Len(), FWarriorGuildName::MaxLength));

	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	const bool bBusy = bWaitingCreate || (Guild && Guild->IsActionInFlight());
	WarriorGuildView::SetButtonEnabled(Button_Create, Check.IsValid() && !bBusy, InactiveButtonColor);

	BP_OnNameChecked(Check.bLengthOk, Check.bCharsOk, Check.bSpacesOk);
}

void UWarriorGuildCreateDialogWidget::HandleCreateClicked()
{
	const FString Name = EditableTextBox_Name ? EditableTextBox_Name->GetText().ToString() : FString();
	if (!FWarriorGuildName::Check(Name).IsValid())
	{
		return;
	}

	UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (Guild && Guild->CreateGuild(Name))
	{
		bWaitingCreate = true;
		SetNameError(FText::GetEmpty());
	}
	UpdateRules();
}

void UWarriorGuildCreateDialogWidget::HandleCancelClicked()
{
	Close();
}

void UWarriorGuildCreateDialogWidget::HandleActionCompleted(EWarriorGuildAction InAction, bool bInSuccess, const FString& InErrorCode, const FText& InMessage)
{
	if (InAction != EWarriorGuildAction::Create || !bWaitingCreate)
	{
		UpdateRules();
		return;
	}
	bWaitingCreate = false;

	if (bInSuccess)
	{
		//만들었다. 팝업은 G4로 내 길드 탭을 그린다
		Close();
		return;
	}

	//공용 팝업으로 보일 오류(이미 길드가 있음 등)는 대화상자를 닫고 팝업으로. 나머지(이름 중복·상한 등)는 이름 아래 한 줄
	if (WarriorGuildView::PresentActionError(this, InErrorCode, InMessage, Text_NameError))
	{
		Close();
		return;
	}
	UpdateRules();
}

void UWarriorGuildCreateDialogWidget::SetNameError(const FText& InMessage)
{
	WarriorGuildView::SetText(Text_NameError, InMessage);
	WarriorGuildView::ShowPanel(Text_NameError, !InMessage.IsEmpty());
}

//──────────────────────────────── 길드 팝업

UWarriorGuildPopupWidget::UWarriorGuildPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NoGuildTitle = LOCTEXT("NoGuildTitle", "길드");
	MemberCountFormat = LOCTEXT("MemberCountFormat", "{0}/{1}");
	MyRequestCountFormat = LOCTEXT("MyRequestCountFormat", "{0}/{1}");
	LoadFailedText = LOCTEXT("LoadFailed", "길드 정보를 불러오지 못했습니다.");
	EmptyFindText = LOCTEXT("EmptyFind", "아직 길드가 없습니다. 첫 길드를 만들어 보세요.");
	EmptySearchText = LOCTEXT("EmptySearch", "이 이름으로 시작하는 길드가 없습니다.");
	EmptyMyRequestsText = LOCTEXT("EmptyMyRequests", "대기 중인 신청이 없습니다. 신청은 3곳까지, 3일 동안 유효합니다.");
	EmptyJoinRequestsText = LOCTEXT("EmptyJoinRequests", "받은 가입 신청이 없습니다.");
	SearchInvalidText = LOCTEXT("SearchInvalid", "길드 이름은 한글·영문·숫자로 찾을 수 있습니다(12자까지).");
	CreateHintFormat = LOCTEXT("CreateHint", "스테이지 {0}을(를) 클리어하면 길드를 만들 수 있습니다.");
	CreateHintJoinedText = LOCTEXT("CreateHintRequests", "길드를 만들면 내 신청은 모두 지워집니다.");
	LeaveConfirmFormat = LOCTEXT("LeaveConfirm", "{0}에서 탈퇴합니다.\n다시 들어오려면 가입 신청을 해야 합니다.");
	LeaveAloneConfirmFormat = LOCTEXT("LeaveAloneConfirm", "길드원이 없어 {0}이(가) 해체됩니다.\n되돌릴 수 없습니다.");
	DisbandConfirmFormat = LOCTEXT("DisbandConfirm", "{0}을(를) 해체합니다.\n길드원 {1}명은 모두 길드에서 나가고, 받은 신청은 지워집니다.\n되돌릴 수 없습니다.");
}

void UWarriorGuildPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (Button_TabFind)
	{
		Button_TabFind->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabFindClicked);
	}
	if (Button_TabMyRequests)
	{
		Button_TabMyRequests->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabMyRequestsClicked);
	}
	if (Button_TabMembers)
	{
		Button_TabMembers->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabMembersClicked);
	}
	if (Button_TabJoinRequests)
	{
		Button_TabJoinRequests->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabJoinRequestsClicked);
	}
	if (Button_Search)
	{
		Button_Search->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSearchClicked);
	}
	if (EditableTextBox_Search)
	{
		EditableTextBox_Search->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleSearchCommitted);
	}
	if (Button_Retry)
	{
		Button_Retry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	}
	if (Button_Create)
	{
		Button_Create->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateClicked);
	}
	if (Button_Leave)
	{
		Button_Leave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeaveClicked);
	}
	if (Button_Disband)
	{
		Button_Disband->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDisbandClicked);
	}
	if (Button_ConfirmYes)
	{
		Button_ConfirmYes->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmYesClicked);
	}
	if (Button_ConfirmNo)
	{
		Button_ConfirmNo->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmNoClicked);
	}
	if (ListView_Guilds)
	{
		ListView_Guilds->OnListViewScrolled().AddUObject(this, &ThisClass::HandleGuildListScrolled);
	}
	WarriorGuildView::SetText(Text_Error, LoadFailedText);
}

void UWarriorGuildPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//팝업을 다시 띄울 때 재사용하므로 화면에 있는 동안만 알림을 받는다
	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->OnMyGuildUpdated.AddUniqueDynamic(this, &ThisClass::HandleMyGuildUpdated);
		Guild->OnMyGuildChanged.AddUniqueDynamic(this, &ThisClass::HandleMyGuildChanged);
		Guild->OnGuildListUpdated.AddUniqueDynamic(this, &ThisClass::HandleGuildListUpdated);
		Guild->OnJoinRequestsUpdated.AddUniqueDynamic(this, &ThisClass::HandleJoinRequestsUpdated);
		Guild->OnGuildActionCompleted.AddUniqueDynamic(this, &ThisClass::HandleActionCompleted);
		Guild->OnRequestFailed.AddUniqueDynamic(this, &ThisClass::HandleRequestFailed);
	}
}

void UWarriorGuildPopupWidget::NativeDestruct()
{
	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->OnMyGuildUpdated.RemoveDynamic(this, &ThisClass::HandleMyGuildUpdated);
		Guild->OnMyGuildChanged.RemoveDynamic(this, &ThisClass::HandleMyGuildChanged);
		Guild->OnGuildListUpdated.RemoveDynamic(this, &ThisClass::HandleGuildListUpdated);
		Guild->OnJoinRequestsUpdated.RemoveDynamic(this, &ThisClass::HandleJoinRequestsUpdated);
		Guild->OnGuildActionCompleted.RemoveDynamic(this, &ThisClass::HandleActionCompleted);
		Guild->OnRequestFailed.RemoveDynamic(this, &ThisClass::HandleRequestFailed);
	}

	Super::NativeDestruct();
}

void UWarriorGuildPopupWidget::Open()
{
	HideConfirm();
	if (CreateDialog)
	{
		CreateDialog->Close();
	}
	SetActionError(FText::GetEmpty());
	bLoadFailed = false;
	bGuildListRequested = false;
	bJoinRequestsRequested = false;
	SearchPrefix.Reset();
	if (EditableTextBox_Search)
	{
		EditableTextBox_Search->SetText(FText::GetEmpty());
	}

	//열 때마다 G4를 다시 받는다. 받은 값이 있으면 먼저 그리고, 새 값이 오면 다시 그린다
	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->RefreshMyGuild();
	}
	SelectTab(GetFirstTab());
}

void UWarriorGuildPopupWidget::Close()
{
	RemoveFromParent();
	OnClosed.Broadcast();
}

//~ Begin 탭

bool UWarriorGuildPopupWidget::IsTabAllowed(EWarriorGuildTab InTab) const
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	const EWarriorGuildRole Role = Guild ? Guild->GetMyGuild().MyRole : EWarriorGuildRole::None;
	switch (Role)
	{
	case EWarriorGuildRole::Leader:
		return InTab == EWarriorGuildTab::Members || InTab == EWarriorGuildTab::JoinRequests;
	case EWarriorGuildRole::Member:
		return InTab == EWarriorGuildTab::Members;
	default:
		return InTab == EWarriorGuildTab::Find || InTab == EWarriorGuildTab::MyRequests;
	}
}

EWarriorGuildTab UWarriorGuildPopupWidget::GetFirstTab() const
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	return Guild && Guild->IsInGuild() ? EWarriorGuildTab::Members : EWarriorGuildTab::Find;
}

void UWarriorGuildPopupWidget::SelectTab(EWarriorGuildTab InTab)
{
	CurrentTab = IsTabAllowed(InTab) ? InTab : GetFirstTab();

	//그 탭의 목록은 이번에 열린 뒤 처음 볼 때 받는다(그다음은 알림·동작 뒤에 서브시스템이 다시 받음)
	UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (Guild && Guild->HasLoadedMyGuild())
	{
		if (CurrentTab == EWarriorGuildTab::Find && !bGuildListRequested)
		{
			bGuildListRequested = true;
			Guild->RequestGuildList(SearchPrefix);
		}
		else if (CurrentTab == EWarriorGuildTab::JoinRequests && !bJoinRequestsRequested)
		{
			bJoinRequestsRequested = true;
			Guild->RequestReceivedJoinRequests();
		}
	}

	Render();
	BP_OnTabSet(CurrentTab);
}

//~ End 탭

//~ Begin 그리기

void UWarriorGuildPopupWidget::Render()
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	const bool bLoaded = Guild && Guild->HasLoadedMyGuild();
	const bool bLoading = !bLoaded && !bLoadFailed;
	const bool bFailed = !bLoaded && bLoadFailed;

	WarriorGuildView::ShowPanel(Widget_Loading, bLoading);
	WarriorGuildView::ShowPanel(Widget_Error, bFailed);
	WarriorGuildView::ShowPanel(Switcher_Tabs, bLoaded);

	RenderTabs();
	RenderList();
	RenderFooter();
	BP_OnViewStateSet(bLoading, bFailed);
}

void UWarriorGuildPopupWidget::RenderTabs()
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	const bool bLoaded = Guild && Guild->HasLoadedMyGuild();
	const FWarriorMyGuild& MyGuild = Guild ? Guild->GetMyGuild() : FWarriorMyGuild();

	WarriorGuildView::SetText(Text_Title, MyGuild.bHasGuild ? FText::FromString(MyGuild.Guild.Name) : NoGuildTitle);
	WarriorGuildView::SetText(Text_MemberCount, FText::Format(MemberCountFormat, MyGuild.Guild.MemberCount, MyGuild.Guild.MaxMembers));
	WarriorGuildView::ShowPanel(Text_MemberCount, MyGuild.bHasGuild);

	WarriorGuildView::ShowButton(Button_TabFind, bLoaded && IsTabAllowed(EWarriorGuildTab::Find));
	WarriorGuildView::ShowButton(Button_TabMyRequests, bLoaded && IsTabAllowed(EWarriorGuildTab::MyRequests));
	WarriorGuildView::ShowButton(Button_TabMembers, bLoaded && IsTabAllowed(EWarriorGuildTab::Members));
	WarriorGuildView::ShowButton(Button_TabJoinRequests, bLoaded && IsTabAllowed(EWarriorGuildTab::JoinRequests));

	WarriorGuildView::SetText(Text_MyRequestCount, FText::Format(MyRequestCountFormat, MyGuild.MyJoinRequests.Num(), MaxPendingRequests));
	WarriorGuildView::SetText(Text_JoinRequestBadge, FText::AsNumber(MyGuild.PendingJoinRequestCount));
	WarriorGuildView::ShowPanel(Text_JoinRequestBadge, MyGuild.PendingJoinRequestCount > 0);

	WarriorGuildView::ShowPanel(Widget_SearchRow, bLoaded && CurrentTab == EWarriorGuildTab::Find);
	if (Switcher_Tabs)
	{
		Switcher_Tabs->SetActiveWidgetIndex(static_cast<int32>(CurrentTab));
	}
}

void UWarriorGuildPopupWidget::RenderList()
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (!Guild || !Guild->HasLoadedMyGuild())
	{
		WarriorGuildView::ShowPanel(Text_Empty, false);
		return;
	}

	const FWarriorMyGuild& MyGuild = Guild->GetMyGuild();
	const bool bIdle = !Guild->IsActionInFlight();
	TArray<UWarriorGuildListItem*> Items;
	auto NewItem = [this, bIdle, &Items](EWarriorGuildListItemKind InKind) -> UWarriorGuildListItem*
	{
		UWarriorGuildListItem* NewEntry = NewObject<UWarriorGuildListItem>(this);
		NewEntry->Kind = InKind;
		NewEntry->bActionsEnabled = bIdle;
		NewEntry->Owner = this;
		Items.Add(NewEntry);
		return NewEntry;
	};

	UListView* ListView = nullptr;
	FText EmptyText;
	switch (CurrentTab)
	{
	case EWarriorGuildTab::Find:
		ListView = ListView_Guilds;
		EmptyText = SearchPrefix.IsEmpty() ? EmptyFindText : EmptySearchText;
		for (const FWarriorGuildSummary& Summary : Guild->GetGuildList())
		{
			UWarriorGuildListItem* Entry = NewItem(EWarriorGuildListItemKind::Guild);
			Entry->Guild = Summary;
			Entry->bApplied = MyGuild.MyJoinRequests.ContainsByPredicate([&Summary](const FWarriorGuildJoinRequest& Request)
			{
				return Request.GuildId == Summary.GuildId;
			});
			//신청이 상한이면 새 신청은 막는다(서버도 GUILD_JOIN_REQUEST_LIMIT로 거절)
			Entry->bBlocked = MyGuild.MyJoinRequests.Num() >= MaxPendingRequests;
		}
		break;

	case EWarriorGuildTab::MyRequests:
		ListView = ListView_MyRequests;
		EmptyText = EmptyMyRequestsText;
		for (const FWarriorGuildJoinRequest& Request : MyGuild.MyJoinRequests)
		{
			NewItem(EWarriorGuildListItemKind::MyRequest)->JoinRequest = Request;
		}
		break;

	case EWarriorGuildTab::Members:
		ListView = ListView_Members;
		for (const FWarriorGuildMember& Member : MyGuild.Members)
		{
			NewItem(EWarriorGuildListItemKind::Member)->Member = Member;
		}
		break;

	case EWarriorGuildTab::JoinRequests:
	{
		ListView = ListView_JoinRequests;
		EmptyText = EmptyJoinRequestsText;
		const bool bFull = MyGuild.Guild.MaxMembers > 0 && MyGuild.Guild.MemberCount >= MyGuild.Guild.MaxMembers;
		for (const FWarriorGuildJoinRequest& Request : Guild->GetReceivedJoinRequests())
		{
			UWarriorGuildListItem* Entry = NewItem(EWarriorGuildListItemKind::ReceivedRequest);
			Entry->JoinRequest = Request;
			Entry->bBlocked = bFull;
		}
		break;
	}
	}

	FillList(ListView, Items);
	WarriorGuildView::SetText(Text_Empty, EmptyText);
	WarriorGuildView::ShowPanel(Text_Empty, Items.IsEmpty() && !EmptyText.IsEmpty());
}

void UWarriorGuildPopupWidget::FillList(UListView* InListView, const TArray<UWarriorGuildListItem*>& InItems)
{
	if (!InListView)
	{
		return;
	}
	//항목 객체를 통째로 바꾼다. 보이는 줄 위젯은 재사용되고 NativeOnListItemObjectSet으로 다시 그려진다
	InListView->SetListItems(InItems);
}

void UWarriorGuildPopupWidget::RenderFooter()
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	const bool bLoaded = Guild && Guild->HasLoadedMyGuild();
	const FWarriorMyGuild& MyGuild = Guild ? Guild->GetMyGuild() : FWarriorMyGuild();
	const bool bIdle = Guild && !Guild->IsActionInFlight();
	const bool bLeader = MyGuild.MyRole == EWarriorGuildRole::Leader;

	//미소속: [길드 만들기](조건 충족일 때만) / 길드원: [탈퇴] / 길드장: [해체](혼자면 [탈퇴]도 해체로 동작)
	const bool bShowCreate = bLoaded && !MyGuild.bHasGuild;
	const bool bShowLeave = bLoaded && MyGuild.bHasGuild && (!bLeader || MyGuild.Guild.MemberCount <= 1);
	const bool bShowDisband = bLoaded && bLeader;

	WarriorGuildView::ShowButton(Button_Create, bShowCreate);
	SetButtonEnabled(Button_Create, MyGuild.bCanCreate && bIdle);
	WarriorGuildView::ShowButton(Button_Leave, bShowLeave);
	SetButtonEnabled(Button_Leave, bIdle);
	WarriorGuildView::ShowButton(Button_Disband, bShowDisband);
	SetButtonEnabled(Button_Disband, bIdle);

	//만들 수 없으면 아직 충족하지 못한 조건, 만들 수 있고 내 신청이 있으면 "신청은 지워짐" 안내
	FText Hint;
	if (bShowCreate && !MyGuild.bCanCreate)
	{
		const FWarriorGuildRequirement* Unmet = MyGuild.CreateRequirements.FindByPredicate([](const FWarriorGuildRequirement& Requirement)
		{
			return !Requirement.bMet;
		});
		if (Unmet)
		{
			Hint = FText::Format(CreateHintFormat, FText::FromName(Unmet->StageId));
		}
	}
	else if (bShowCreate && !MyGuild.MyJoinRequests.IsEmpty())
	{
		Hint = CreateHintJoinedText;
	}
	WarriorGuildView::SetText(Text_CreateHint, Hint);
	WarriorGuildView::ShowPanel(Text_CreateHint, !Hint.IsEmpty());
}

void UWarriorGuildPopupWidget::SetButtonEnabled(UButton* InButton, bool bInEnabled) const
{
	WarriorGuildView::SetButtonEnabled(InButton, bInEnabled, InactiveButtonColor);
}

void UWarriorGuildPopupWidget::SetActionError(const FText& InMessage)
{
	WarriorGuildView::SetText(Text_ActionError, InMessage);
	WarriorGuildView::ShowPanel(Text_ActionError, !InMessage.IsEmpty());
}

//~ End 그리기

//~ Begin 길드 서브시스템 알림

void UWarriorGuildPopupWidget::HandleMyGuildUpdated()
{
	bLoadFailed = false;
	//소속이 바뀌어 지금 탭을 못 쓰면 첫 탭으로. 처음 받은 G4라면 탭 목록도 이때 받는다
	SelectTab(CurrentTab);
}

void UWarriorGuildPopupWidget::HandleMyGuildChanged(const FString& InOldGuildId, const FString& InNewGuildId)
{
	//가입·만들기·탈퇴·해체: 새 소속의 첫 탭부터. 확인 칸은 더 이상 맞지 않으므로 닫는다
	HideConfirm();
	bJoinRequestsRequested = false;
	bGuildListRequested = false;
	CurrentTab = GetFirstTab();
}

void UWarriorGuildPopupWidget::HandleGuildListUpdated()
{
	if (CurrentTab == EWarriorGuildTab::Find)
	{
		Render();
	}
}

void UWarriorGuildPopupWidget::HandleJoinRequestsUpdated()
{
	if (CurrentTab == EWarriorGuildTab::JoinRequests)
	{
		Render();
	}
}

void UWarriorGuildPopupWidget::HandleActionCompleted(EWarriorGuildAction InAction, bool bInSuccess, const FString& InErrorCode, const FText& InMessage)
{
	//만들기 대화상자가 떠 있으면 만들기 결과는 대화상자가 보인다
	const bool bDialogHandles = InAction == EWarriorGuildAction::Create && CreateDialog && CreateDialog->IsOpen();

	if (bInSuccess)
	{
		SetActionError(FText::GetEmpty());
	}
	else if (!bDialogHandles)
	{
		HideConfirm();
		SetActionError(FText::GetEmpty());
		WarriorGuildView::PresentActionError(this, InErrorCode, InMessage, Text_ActionError);
		WarriorGuildView::ShowPanel(Text_ActionError, Text_ActionError && !Text_ActionError->GetText().IsEmpty());
	}

	//쓰는 중에 막았던 버튼을 푼다
	Render();
}

void UWarriorGuildPopupWidget::HandleRequestFailed(const FString& InErrorCode, const FText& InMessage)
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (!Guild || !Guild->HasLoadedMyGuild())
	{
		//G4를 한 번도 못 받았으면 탭 자리를 실패 표시로(다시 불러오기)
		bLoadFailed = true;
	}
	else
	{
		SetActionError(InMessage);
	}
	Render();
}

//~ End 길드 서브시스템 알림

//~ Begin 버튼

void UWarriorGuildPopupWidget::HandleCloseClicked()
{
	Close();
}

void UWarriorGuildPopupWidget::HandleTabFindClicked()
{
	SetActionError(FText::GetEmpty());
	SelectTab(EWarriorGuildTab::Find);
}

void UWarriorGuildPopupWidget::HandleTabMyRequestsClicked()
{
	SetActionError(FText::GetEmpty());
	SelectTab(EWarriorGuildTab::MyRequests);
}

void UWarriorGuildPopupWidget::HandleTabMembersClicked()
{
	SetActionError(FText::GetEmpty());
	SelectTab(EWarriorGuildTab::Members);
}

void UWarriorGuildPopupWidget::HandleTabJoinRequestsClicked()
{
	SetActionError(FText::GetEmpty());
	SelectTab(EWarriorGuildTab::JoinRequests);
}

void UWarriorGuildPopupWidget::HandleSearchClicked()
{
	SendSearch();
}

void UWarriorGuildPopupWidget::HandleSearchCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	if (InCommitMethod == ETextCommit::OnEnter)
	{
		SendSearch();
	}
}

void UWarriorGuildPopupWidget::SendSearch()
{
	const FString Prefix = EditableTextBox_Search ? EditableTextBox_Search->GetText().ToString() : FString();

	//서버와 같은 규칙으로 먼저 거른다(맞지 않으면 400). 빈 칸은 전체 목록
	if (!Prefix.IsEmpty() && !FWarriorGuildName::IsValidSearchPrefix(Prefix))
	{
		SetActionError(SearchInvalidText);
		return;
	}

	SetActionError(FText::GetEmpty());
	SearchPrefix = Prefix;
	bGuildListRequested = true;
	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->RequestGuildList(SearchPrefix);
	}
}

void UWarriorGuildPopupWidget::HandleRetryClicked()
{
	bLoadFailed = false;
	if (UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this))
	{
		Guild->RefreshMyGuild();
	}
	Render();
}

void UWarriorGuildPopupWidget::HandleCreateClicked()
{
	SetActionError(FText::GetEmpty());
	if (CreateDialog)
	{
		CreateDialog->Open();
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Guild] %s has no CreateDialog. Guild cannot be created from this popup."), *GetName());
	}
}

void UWarriorGuildPopupWidget::HandleLeaveClicked()
{
	ShowConfirm(EWarriorGuildAction::Leave);
}

void UWarriorGuildPopupWidget::HandleDisbandClicked()
{
	ShowConfirm(EWarriorGuildAction::Disband);
}

void UWarriorGuildPopupWidget::HandleConfirmYesClicked()
{
	const bool bWasConfirming = bConfirming;
	const EWarriorGuildAction Action = ConfirmAction;
	HideConfirm();

	UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (!bWasConfirming || !Guild)
	{
		return;
	}
	SetActionError(FText::GetEmpty());
	if (Action == EWarriorGuildAction::Disband)
	{
		Guild->DisbandGuild();
	}
	else
	{
		Guild->LeaveGuild();
	}
	Render();
}

void UWarriorGuildPopupWidget::HandleConfirmNoClicked()
{
	HideConfirm();
}

//~ End 버튼

void UWarriorGuildPopupWidget::ShowConfirm(EWarriorGuildAction InAction)
{
	const UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (!Guild || !Guild->IsInGuild())
	{
		return;
	}

	//확인 칸이 없는 WBP면 묻지 않고 보내지 않는다(실수로 해체되지 않게)
	if (!Widget_Confirm)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Guild] %s has no Widget_Confirm. Leave/disband is not sent."), *GetName());
		return;
	}

	const FWarriorMyGuild& MyGuild = Guild->GetMyGuild();
	const FText GuildName = FText::FromString(MyGuild.Guild.Name);
	const bool bLeaderAlone = MyGuild.MyRole == EWarriorGuildRole::Leader && MyGuild.Guild.MemberCount <= 1;

	FText Message;
	if (InAction == EWarriorGuildAction::Disband)
	{
		Message = FText::Format(DisbandConfirmFormat, GuildName, MyGuild.Guild.MemberCount);
	}
	else
	{
		Message = FText::Format(bLeaderAlone ? LeaveAloneConfirmFormat : LeaveConfirmFormat, GuildName);
	}

	bConfirming = true;
	ConfirmAction = InAction;
	WarriorGuildView::SetText(Text_Confirm, Message);
	Widget_Confirm->SetVisibility(ESlateVisibility::Visible);
}

void UWarriorGuildPopupWidget::HideConfirm()
{
	bConfirming = false;
	if (Widget_Confirm)
	{
		Widget_Confirm->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWarriorGuildPopupWidget::HandleRowAction(const UWarriorGuildListItem* InItem, EWarriorGuildAction InAction)
{
	UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (!InItem || !Guild)
	{
		return;
	}

	SetActionError(FText::GetEmpty());
	switch (InAction)
	{
	case EWarriorGuildAction::Apply:
		Guild->ApplyToGuild(InItem->Guild.GuildId);
		break;
	case EWarriorGuildAction::Cancel:
		Guild->CancelJoinRequest(InItem->JoinRequest.JoinRequestId);
		break;
	case EWarriorGuildAction::Approve:
		Guild->ApproveJoinRequest(InItem->JoinRequest.JoinRequestId);
		break;
	case EWarriorGuildAction::Reject:
		Guild->RejectJoinRequest(InItem->JoinRequest.JoinRequestId);
		break;
	default:
		return;
	}

	//쓰는 동안 줄 버튼을 막는다(끝나면 HandleActionCompleted가 푼다)
	Render();
}

void UWarriorGuildPopupWidget::HandleGuildListScrolled(float InItemOffset, float InDistanceRemaining)
{
	if (CurrentTab != EWarriorGuildTab::Find || InDistanceRemaining > LoadMoreThreshold)
	{
		return;
	}

	UWarriorGuildSubsystem* Guild = UWarriorGuildSubsystem::Get(this);
	if (!Guild || !Guild->HasMoreGuilds())
	{
		return;
	}

	//빠른 스크롤에도 최소 간격보다 자주 보내지 않는다. 받는 중이면 서브시스템이 무시한다
	const double Now = FPlatformTime::Seconds();
	if (Now - LastLoadMoreSeconds < LoadMoreMinInterval)
	{
		return;
	}
	LastLoadMoreSeconds = Now;
	Guild->RequestMoreGuilds();
}

#undef LOCTEXT_NAMESPACE
