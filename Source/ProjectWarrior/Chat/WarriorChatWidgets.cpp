// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorChatWidgets.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Chat/WarriorChatRules.h"
#include "ProjectWarrior/Chat/WarriorChatSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"

#define LOCTEXT_NAMESPACE "WarriorChatWidgets"

namespace WarriorChatWidgets
{
	//서버 시각은 UTC. PC 시간대로 바꾼다(랭킹 FormatDate와 같은 방식)
	FTimespan UtcToLocal()
	{
		return FDateTime::Now() - FDateTime::UtcNow();
	}

	void SetText(UTextBlock* InText, const FText& InValue)
	{
		if (InText)
		{
			InText->SetText(InValue);
		}
	}

	void SetShown(UWidget* InWidget, bool bInShown)
	{
		if (InWidget)
		{
			InWidget->SetVisibility(bInShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	FString MyAccountId(const UObject* InContext)
	{
		const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(InContext);
		return Auth && Auth->IsLoggedIn() ? Auth->GetAccount().AccountId : FString();
	}
}

//~ Begin UWarriorChatLineWidget

UWarriorChatLineWidget::UWarriorChatLineWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DateLineFormat = LOCTEXT("DateLineFormat", "{0}월 {1}일 ({2})");
	SendingText = LOCTEXT("Sending", "보내는 중");
	FailedText = LOCTEXT("Failed", "보내지 못함");
}

void UWarriorChatLineWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Retry)
	{
		Button_Retry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	}
}

void UWarriorChatLineWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	using namespace WarriorChatWidgets;

	//ListView가 줄 위젯을 재사용하므로 매번 모든 칸을 다시 넣는다
	Item = Cast<UWarriorChatRowItem>(ListItemObject);
	if (!Item)
	{
		return;
	}

	SetShown(Panel_Date, Item->bDateLine);
	SetShown(Panel_Line, !Item->bDateLine);
	if (Item->bDateLine)
	{
		SetText(Text_Date, FText::Format(DateLineFormat, FText::AsNumber(Item->LocalDate.GetMonth()),
			FText::AsNumber(Item->LocalDate.GetDay()), FText::FromString(WarriorChatRules::WeekdayKorean(Item->LocalDate))));
		SetShown(Button_Retry, false);
		BP_OnLineSet(true, false, EWarriorChatLineState::Sent);
		return;
	}

	const FWarriorChatLine& Line = Item->Line;
	const FDateTime Local = Line.Message.SentAt + UtcToLocal();
	SetText(Text_Time, FText::FromString(Local.ToString(TEXT("%H:%M"))));
	SetText(Text_Nickname, FText::FromString(Line.Message.SenderNickname));
	SetText(Text_Message, FText::FromString(Line.Message.Text));

	const bool bFailed = Line.State == EWarriorChatLineState::Failed;
	SetShown(Text_State, Line.State != EWarriorChatLineState::Sent);
	SetText(Text_State, bFailed ? FailedText : SendingText);
	if (Button_Retry)
	{
		Button_Retry->SetVisibility(bFailed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	BP_OnLineSet(false, Item->bMine, Line.State);
}

void UWarriorChatLineWidget::HandleRetryClicked()
{
	if (!Item || Item->bDateLine)
	{
		return;
	}
	if (UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this))
	{
		Chat->RetryLine(Item->Scope, Item->Line.RequestId);
	}
}

//~ End UWarriorChatLineWidget

//~ Begin UWarriorChatPanelWidget

UWarriorChatPanelWidget::UWarriorChatPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SendText = LOCTEXT("Send", "보내기");
	SendBlockedFormat = LOCTEXT("SendBlocked", "보내기 ({0})");
	EmptyText = LOCTEXT("Empty", "아직 채팅이 없습니다.");
	GuildNoneText = LOCTEXT("GuildNone", "길드에 가입하면 길드 채팅을 볼 수 있습니다.");
	TextInvalidText = LOCTEXT("TextInvalid", "글을 확인해 주세요.");
}

void UWarriorChatPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_TabGlobal)
	{
		Button_TabGlobal->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabGlobalClicked);
	}
	if (Button_TabGuild)
	{
		Button_TabGuild->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabGuildClicked);
	}
	if (Button_Send)
	{
		Button_Send->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSendClicked);
	}
	if (EditableTextBox_Input)
	{
		EditableTextBox_Input->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleInputChanged);
		EditableTextBox_Input->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleInputCommitted);
	}
	if (ListView_Lines)
	{
		ListView_Lines->OnListViewScrolled().AddUObject(this, &ThisClass::HandleListScrolled);
	}
}

void UWarriorChatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//창이 떠 있는 동안만 알림을 받는다. 줄은 서브시스템에 남아 있어 다시 열면 그대로 보인다
	if (UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this))
	{
		Chat->OnChatUpdated.AddUniqueDynamic(this, &ThisClass::HandleChatUpdated);
		Chat->OnSendStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSendStateChanged);
		Chat->OnSendRejected.AddUniqueDynamic(this, &ThisClass::HandleSendRejected);
		Chat->OnGuildStateChanged.AddUniqueDynamic(this, &ThisClass::HandleGuildStateChanged);
	}
	SetNotice(FText::GetEmpty());
	SelectTab(SelectedScope);
}

void UWarriorChatPanelWidget::NativeDestruct()
{
	if (UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this))
	{
		Chat->OnChatUpdated.RemoveDynamic(this, &ThisClass::HandleChatUpdated);
		Chat->OnSendStateChanged.RemoveDynamic(this, &ThisClass::HandleSendStateChanged);
		Chat->OnSendRejected.RemoveDynamic(this, &ThisClass::HandleSendRejected);
		Chat->OnGuildStateChanged.RemoveDynamic(this, &ThisClass::HandleGuildStateChanged);
	}

	Super::NativeDestruct();
}

void UWarriorChatPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	//429 남은 초가 바뀔 때만 버튼 글자를 고친다
	const UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	if (Chat && Chat->GetSendBlockedSeconds() != ShownBlockedSeconds)
	{
		UpdateSendButton();
	}
}

void UWarriorChatPanelWidget::SelectTab(EWarriorChatScope InScope)
{
	SelectedScope = InScope;
	bAtBottom = true;
	SetNotice(FText::GetEmpty());
	Rebuild(true);
	UpdateSendButton();
	BP_OnTabSelected(InScope);

	//창을 열 때 길드 내역 첫 묶음(chat-api.md "놓친 길드 채팅"). 길드 없음이면 묻지 않는다
	UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	if (InScope == EWarriorChatScope::Guild && Chat && Chat->GetGuildState() != EWarriorChatGuildState::None
		&& !Chat->IsLoadingGuildHistory())
	{
		Chat->LoadGuildHistory(false);
	}
}

void UWarriorChatPanelWidget::SendInput()
{
	UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	if (!Chat || !EditableTextBox_Input)
	{
		return;
	}
	const FString Text = EditableTextBox_Input->GetText().ToString();
	if (Text.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	//보내면 서브시스템이 OnChatUpdated를 바로 알린다: 그 다시 그리기는 맨 아래로(내 줄)
	bAtBottom = true;
	switch (Chat->SendMessage(SelectedScope, Text))
	{
	case EWarriorChatSendResult::Sending:
		EditableTextBox_Input->SetText(FText::GetEmpty());
		SetNotice(FText::GetEmpty());
		break;
	case EWarriorChatSendResult::Empty:
		break;
	case EWarriorChatSendResult::TooLong:
	case EWarriorChatSendResult::ControlChar:
		SetNotice(TextInvalidText);
		break;
	case EWarriorChatSendResult::Blocked:
		//안내는 막힐 때 이미 보였다
		break;
	default:
		SetNotice(FWarriorErrorText::GetText(TEXT("UNKNOWN"), WarriorErrorContext::Chat));
		break;
	}
}

void UWarriorChatPanelWidget::HandleTabGlobalClicked()
{
	SelectTab(EWarriorChatScope::Global);
}

void UWarriorChatPanelWidget::HandleTabGuildClicked()
{
	SelectTab(EWarriorChatScope::Guild);
}

void UWarriorChatPanelWidget::HandleSendClicked()
{
	SendInput();
}

void UWarriorChatPanelWidget::HandleInputChanged(const FText& InText)
{
	if (bTrimmingInput || !EditableTextBox_Input)
	{
		return;
	}
	//입력 칸도 코드 포인트로 막는다(최종 판정은 서버, chat-api.md "게임 처리 규칙")
	const UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	const int32 MaxLength = Chat ? Chat->GetMaxTextLength() : 100;
	const FString Text = InText.ToString();
	if (WarriorChatRules::CountCodePoints(Text) > MaxLength)
	{
		TGuardValue<bool> Guard(bTrimmingInput, true);
		EditableTextBox_Input->SetText(FText::FromString(WarriorChatRules::TruncateToCodePoints(Text, MaxLength)));
	}
}

void UWarriorChatPanelWidget::HandleInputCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	if (InCommitMethod != ETextCommit::OnEnter)
	{
		return;
	}
	SendInput();
	//Enter 뒤에도 이어 칠 수 있게 포커스를 돌려준다
	if (EditableTextBox_Input)
	{
		EditableTextBox_Input->SetKeyboardFocus();
	}
}

void UWarriorChatPanelWidget::HandleChatUpdated(EWarriorChatScope InScope)
{
	if (InScope == SelectedScope)
	{
		Rebuild(false);
	}
}

void UWarriorChatPanelWidget::HandleSendStateChanged(bool bInBlocked, const FText& InNotice)
{
	SetNotice(bInBlocked ? InNotice : FText::GetEmpty());
	UpdateSendButton();
	BP_OnSendBlockedChanged(bInBlocked);
}

void UWarriorChatPanelWidget::HandleSendRejected(EWarriorChatScope InScope, const FString& InCode, const FText& InMessage,
	const FString& InText)
{
	//받지 않은 글은 입력 칸에 되돌린다(그사이 새로 친 글이 있으면 덮지 않음)
	if (EditableTextBox_Input && EditableTextBox_Input->GetText().IsEmpty())
	{
		EditableTextBox_Input->SetText(FText::FromString(InText));
	}
	SetNotice(InMessage);
}

void UWarriorChatPanelWidget::HandleGuildStateChanged(EWarriorChatGuildState InState)
{
	UpdateEmptyText();
	UpdateSendButton();
}

void UWarriorChatPanelWidget::HandleListScrolled(float InItemOffset, float InDistanceRemaining)
{
	bAtBottom = InDistanceRemaining <= FollowBottomThreshold;

	//길드 탭 맨 위: 이전 묶음. 받는 중·더 없음이면 서브시스템이 거절한다
	if (SelectedScope == EWarriorChatScope::Guild && InItemOffset <= LoadOlderOffset)
	{
		if (UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this))
		{
			if (Chat->HasOlderGuildHistory() && !Chat->IsLoadingGuildHistory())
			{
				Chat->LoadGuildHistory(true);
			}
		}
	}
}

void UWarriorChatPanelWidget::Rebuild(bool bInForceBottom)
{
	const UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	if (!ListView_Lines || !Chat)
	{
		UpdateEmptyText();
		return;
	}

	//위에 줄이 붙어도 보던 줄을 지키려고, 지금 맨 위에 보이는 채팅 줄의 번호와 위치를 잡아 둔다
	const float PreviousOffset = ListView_Lines->GetScrollOffset();
	FString AnchorId;
	int32 AnchorIndex = INDEX_NONE;
	const TArray<UObject*>& OldItems = ListView_Lines->GetListItems();
	for (int32 Index = FMath::Max(0, FMath::FloorToInt32(PreviousOffset)); Index < OldItems.Num(); ++Index)
	{
		const UWarriorChatRowItem* Old = Cast<UWarriorChatRowItem>(OldItems[Index]);
		if (Old && !Old->bDateLine && !Old->Line.Message.MessageId.IsEmpty())
		{
			AnchorId = Old->Line.Message.MessageId;
			AnchorIndex = Index;
			break;
		}
	}

	const TArray<FWarriorChatLine> Lines = Chat->GetLines(SelectedScope);
	const FString MyId = WarriorChatWidgets::MyAccountId(this);
	const TArray<FWarriorChatRow> Rows = WarriorChatRules::BuildRows(Lines, WarriorChatWidgets::UtcToLocal(), FDateTime::Now());

	TArray<UWarriorChatRowItem*> Items;
	Items.Reserve(Rows.Num());
	int32 NewAnchorIndex = INDEX_NONE;
	for (const FWarriorChatRow& Row : Rows)
	{
		UWarriorChatRowItem* NewItem = NewObject<UWarriorChatRowItem>(this);
		NewItem->bDateLine = Row.bDateLine;
		NewItem->LocalDate = Row.LocalDate;
		NewItem->Scope = SelectedScope;
		if (!Row.bDateLine)
		{
			NewItem->Line = Lines[Row.LineIndex];
			NewItem->bMine = NewItem->Line.State != EWarriorChatLineState::Sent
				|| (!MyId.IsEmpty() && NewItem->Line.Message.SenderAccountId == MyId);
			if (!AnchorId.IsEmpty() && NewItem->Line.Message.MessageId == AnchorId)
			{
				NewAnchorIndex = Items.Num();
			}
		}
		Items.Add(NewItem);
	}
	ListView_Lines->SetListItems(Items);

	if (bInForceBottom || bAtBottom)
	{
		//맨 아래를 보고 있었거나 내가 보냄: 새 줄을 따라 내려간다
		ListView_Lines->ScrollToBottom();
		bAtBottom = true;
	}
	else if (AnchorIndex != INDEX_NONE && NewAnchorIndex != INDEX_NONE)
	{
		//위에 붙은 만큼(이전 묶음·날짜선) 내려 보던 줄을 그 자리에 둔다
		ListView_Lines->SetScrollOffset(PreviousOffset + (NewAnchorIndex - AnchorIndex));
	}
	else
	{
		ListView_Lines->SetScrollOffset(PreviousOffset);
	}
	UpdateEmptyText();
}

void UWarriorChatPanelWidget::UpdateEmptyText()
{
	const UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	const bool bGuildNone = SelectedScope == EWarriorChatScope::Guild && Chat && Chat->GetGuildState() == EWarriorChatGuildState::None;
	const bool bEmpty = !Chat || Chat->GetLines(SelectedScope).IsEmpty();
	WarriorChatWidgets::SetText(Text_Empty, bGuildNone ? GuildNoneText : EmptyText);
	WarriorChatWidgets::SetShown(Text_Empty, bGuildNone || bEmpty);
}

void UWarriorChatPanelWidget::UpdateSendButton()
{
	const UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(this);
	const int32 Seconds = Chat ? Chat->GetSendBlockedSeconds() : 0;
	ShownBlockedSeconds = Seconds;
	const bool bGuildNone = SelectedScope == EWarriorChatScope::Guild && Chat && Chat->GetGuildState() == EWarriorChatGuildState::None;
	if (Button_Send)
	{
		Button_Send->SetIsEnabled(Chat && Seconds == 0 && !bGuildNone);
	}
	WarriorChatWidgets::SetText(Text_Send, Seconds > 0 ? FText::Format(SendBlockedFormat, FText::AsNumber(Seconds)) : SendText);
}

void UWarriorChatPanelWidget::SetNotice(const FText& InNotice)
{
	WarriorChatWidgets::SetText(Text_Notice, InNotice);
	WarriorChatWidgets::SetShown(Text_Notice, !InNotice.IsEmpty());
}

//~ End UWarriorChatPanelWidget

#undef LOCTEXT_NAMESPACE
