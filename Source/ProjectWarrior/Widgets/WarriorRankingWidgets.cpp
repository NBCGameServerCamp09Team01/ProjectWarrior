// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorRankingWidgets.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ListView.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Network/WarriorRankingSubsystem.h"
#include "ProjectWarrior/Network/WarriorStageListSubsystem.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"

#define LOCTEXT_NAMESPACE "WarriorRanking"

namespace WarriorRankingView
{
	//클리어 시간 "3:03.4"
	FText FormatPlayTime(float InSeconds)
	{
		const int32 TotalTenths = FMath::RoundToInt(FMath::Max(0.f, InSeconds) * 10.f);
		const int32 Minutes = TotalTenths / 600;
		const int32 Seconds = (TotalTenths / 10) % 60;
		const int32 Tenths = TotalTenths % 10;
		return FText::FromString(FString::Printf(TEXT("%d:%02d.%d"), Minutes, Seconds, Tenths));
	}

	//서버 시각은 UTC. 날짜는 이 PC의 시간대로 보인다(스테이지 선택의 클리어 날짜와 같은 방식)
	FText FormatDate(const FDateTime& InUtc)
	{
		if (InUtc == FDateTime())
		{
			return FText::GetEmpty();
		}
		const FDateTime LocalTime = InUtc + (FDateTime::Now() - FDateTime::UtcNow());
		return FText::FromString(LocalTime.ToString(TEXT("%m/%d")));
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
}

//──────────────────────────────── 줄

UWarriorRankingRowWidget::UWarriorRankingRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScoreFormat = LOCTEXT("ScoreFormat", "{0}점");
	ClearedFormat = LOCTEXT("ClearedFormat", "{0}개 클리어");
	MineName = LOCTEXT("MineName", "나");
	//도안 2.1판 색(금·은·동), 보통 순위는 StageLabelText 색
	GoldColor = FSlateColor(FLinearColor(0.768f, 0.479f, 0.077f, 1.f));
	SilverColor = FSlateColor(FLinearColor(0.571f, 0.584f, 0.604f, 1.f));
	BronzeColor = FSlateColor(FLinearColor(0.558f, 0.227f, 0.102f, 1.f));
	RankColor = FSlateColor(FLinearColor(0.871f, 0.301f, 0.198f, 1.f));
}

void UWarriorRankingRowWidget::SetEntry(const FWarriorRankingEntry& InEntry, EWarriorRankingBoardKind InKind)
{
	using namespace WarriorRankingView;

	SetText(Text_Rank, FText::AsNumber(InEntry.Rank));
	SetText(Text_Nickname, InEntry.bIsMine && InEntry.Nickname.IsEmpty() ? MineName : FText::FromString(InEntry.Nickname));
	SetText(Text_Score, FText::Format(ScoreFormat, FText::AsNumber(InEntry.Score)));

	FText Detail;
	if (InKind == EWarriorRankingBoardKind::Stage && InEntry.PlayTimeSeconds >= 0.f)
	{
		Detail = FormatPlayTime(InEntry.PlayTimeSeconds);
	}
	else if (InEntry.ClearedStageCount >= 0)
	{
		Detail = FText::Format(ClearedFormat, InEntry.ClearedStageCount);
	}
	SetText(Text_Detail, Detail);
	SetText(Text_Date, FormatDate(InEntry.AchievedAt));

	if (Text_Rank)
	{
		Text_Rank->SetColorAndOpacity(InEntry.Rank == 1 ? GoldColor
			: InEntry.Rank == 2 ? SilverColor
			: InEntry.Rank == 3 ? BronzeColor : RankColor);
	}

	ApplyRowLook(InEntry.Rank, InEntry.bIsMine);
	BP_OnRowSet(InEntry.Rank, InEntry.bIsMine, true);
}

void UWarriorRankingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Border_Row)
	{
		DefaultBrush = Border_Row->Background;
		DefaultBrushColor = Border_Row->GetBrushColor();
		bHasDefaultBrush = true;
	}
}

void UWarriorRankingRowWidget::ApplyRowLook(int32 InRank, bool bInIsMine)
{
	if (!Border_Row || !bHasDefaultBrush)
	{
		return;
	}

	//ListView가 줄 위젯을 재사용하므로 늘 기본 모양에서 시작한다
	FSlateBrush Brush = DefaultBrush;
	FLinearColor BrushColor = DefaultBrushColor;
	if (bInIsMine)
	{
		Brush.OutlineSettings.Color = FSlateColor(MineOutlineColor);
		BrushColor = MineBackgroundColor;
	}
	else if (InRank >= 1 && InRank <= 3)
	{
		const FSlateColor& Medal = InRank == 1 ? GoldColor : InRank == 2 ? SilverColor : BronzeColor;
		Brush.OutlineSettings.Color = Medal;
	}
	Border_Row->SetBrush(Brush);
	Border_Row->SetBrushColor(BrushColor);
}

void UWarriorRankingRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	//ListView가 줄 위젯을 재사용할 때마다 다른 항목을 넣는다
	if (const UWarriorRankingEntryItem* Item = Cast<UWarriorRankingEntryItem>(ListItemObject))
	{
		SetEntry(Item->Entry, Item->Kind);
	}
}

void UWarriorRankingRowWidget::SetNoRecord(const FText& InMessage)
{
	using namespace WarriorRankingView;

	SetText(Text_Rank, FText::FromString(TEXT("-")));
	SetText(Text_Nickname, MineName);
	SetText(Text_Score, InMessage);
	SetText(Text_Detail, FText::GetEmpty());
	SetText(Text_Date, FText::GetEmpty());
	if (Text_Rank)
	{
		Text_Rank->SetColorAndOpacity(RankColor);
	}

	ApplyRowLook(0, true);
	BP_OnRowSet(0, true, false);
}

//──────────────────────────────── 보기 공통

UWarriorRankingViewWidget::UWarriorRankingViewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	ErrorText = LOCTEXT("ErrorText", "현재 서버에서 데이터를 불러올 수가 없습니다.");
	RefreshText = LOCTEXT("RefreshText", "새로 고침");
	RetryText = LOCTEXT("RetryText", "다시 불러오기");
	CooldownFormat = LOCTEXT("CooldownFormat", "{0} ({1})");
	LoadingRefreshText = LOCTEXT("LoadingRefreshText", "불러오는 중…");
	EmptyText = LOCTEXT("EmptyText", "아직 기록이 없습니다. 스테이지를 클리어하면 순위에 올라갑니다.");
	NoRecordText = LOCTEXT("NoRecordText", "기록 없음");
}

void UWarriorRankingViewWidget::Close()
{
	RemoveFromParent();
	OnClosed.Broadcast();
}

void UWarriorRankingViewWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefreshClicked);
	}
	if (Button_Retry)
	{
		Button_Retry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefreshClicked);
	}
	if (Button_More)
	{
		Button_More->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMoreClicked);
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	WarriorRankingView::SetText(Text_Error, ErrorText);
}

void UWarriorRankingViewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//화면을 다시 띄울 때 재사용하므로 화면에 있는 동안만 알림을 받는다
	if (UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this))
	{
		Ranking->OnBoardUpdated.AddUniqueDynamic(this, &ThisClass::HandleBoardUpdated);
		Ranking->OnBoardFailed.AddUniqueDynamic(this, &ThisClass::HandleBoardFailed);
	}
}

void UWarriorRankingViewWidget::NativeDestruct()
{
	if (UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this))
	{
		Ranking->OnBoardUpdated.RemoveDynamic(this, &ThisClass::HandleBoardUpdated);
		Ranking->OnBoardFailed.RemoveDynamic(this, &ThisClass::HandleBoardFailed);
	}

	Super::NativeDestruct();
}

void UWarriorRankingViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateRefreshButtons(false);
}

void UWarriorRankingViewWidget::RequestView(EWarriorRankingBoardKind InKind, FName InTargetId, EWarriorRankingPeriod InPeriod, int32 InLimit, int32 InDifficulty)
{
	UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this);
	if (!Ranking)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Ranking] No ranking subsystem."));
		SetFailed(true);
		return;
	}

	ViewKey.Kind = InKind;
	ViewKey.TargetId = (InKind == EWarriorRankingBoardKind::Stage || InKind == EWarriorRankingBoardKind::Region) ? InTargetId : NAME_None;
	ViewKey.Period = InKind == EWarriorRankingBoardKind::Progress ? EWarriorRankingPeriod::All : InPeriod;
	//서브시스템과 같은 규칙(진행 보드는 난이도 0). 다르면 IsMyBoard가 응답을 버린다
	ViewKey.Difficulty = InKind == EWarriorRankingBoardKind::Progress ? 0 : FMath::Max(0, InDifficulty);
	ViewLimit = InLimit;
	AppendBlockedUntilSeconds = 0.0;
	bHasView = true;

	SetFailed(false);
	SetLoading(true);
	Ranking->RequestBoard(InKind, InTargetId, InPeriod, InLimit, ViewKey.Difficulty);
}

bool UWarriorRankingViewWidget::IsMyBoard() const
{
	const UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this);
	return bHasView && Ranking && Ranking->GetBoardKey() == ViewKey;
}

void UWarriorRankingViewWidget::HandleBoardUpdated()
{
	if (!IsMyBoard())
	{
		return;
	}

	//"더 보기"(스크롤 자동 받기)가 끝난 것도 여기로 온다. 쿨타임은 첫 묶음(새로 고침)에서만 다시 건다
	const bool bFirstPage = bLoading;
	if (bFirstPage)
	{
		Cooldown.Start(FPlatformTime::Seconds(), RefreshCooldownAfterSuccess);
	}
	SetLoading(false);
	SetFailed(false);
	RebuildRows(bFirstPage);
	UpdateRefreshButtons(true);
}

void UWarriorRankingViewWidget::HandleBoardFailed(EWarriorRankingFailure InFailure, const FString& InErrorCode)
{
	if (!IsMyBoard())
	{
		return;
	}

	//첫 묶음(받는 중)이 아니면 스크롤로 다음 묶음을 받다 실패한 것
	const bool bAppendFailed = !bLoading;
	SetLoading(false);
	if (InFailure == EWarriorRankingFailure::SessionEnded)
	{
		//로그인 상태가 끝났다(Auth가 로그인 화면으로 보낸다). 이 화면은 닫는다
		Close();
		return;
	}

	if (bAppendFailed)
	{
		//이미 받은 줄은 그대로 두고, 실패 뒤 쿨타임 동안 다음 묶음을 다시 받지 않는다
		AppendBlockedUntilSeconds = FPlatformTime::Seconds() + RefreshCooldownAfterFailure;
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Ranking] Next page failed. code=%s"), *InErrorCode);
		return;
	}

	SetFailed(true);
	Cooldown.Start(FPlatformTime::Seconds(), RefreshCooldownAfterFailure);
	UpdateRefreshButtons(true);
}

void UWarriorRankingViewWidget::HandleRefreshClicked()
{
	if (!bHasView || bLoading || !Cooldown.IsReady(FPlatformTime::Seconds()))
	{
		return;
	}
	RequestView(ViewKey.Kind, ViewKey.TargetId, ViewKey.Period, ViewLimit, ViewKey.Difficulty);
	UpdateRefreshButtons(true);
}

void UWarriorRankingViewWidget::HandleMoreClicked()
{
	if (UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this))
	{
		if (IsMyBoard())
		{
			Ranking->RequestMore();
		}
	}
}

void UWarriorRankingViewWidget::HandleCloseClicked()
{
	Close();
}

void UWarriorRankingViewWidget::RebuildRows(bool bInFirstPage)
{
	const UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this);
	if (!Ranking)
	{
		return;
	}

	const TArray<FWarriorRankingEntry>& Entries = Ranking->GetEntries();
	if (Panel_Rows)
	{
		Panel_Rows->ClearChildren();
		if (RowClass)
		{
			for (const FWarriorRankingEntry& Entry : Entries)
			{
				if (UWarriorRankingRowWidget* Row = CreateWidget<UWarriorRankingRowWidget>(this, RowClass))
				{
					Row->SetEntry(Entry, ViewKey.Kind);
					Panel_Rows->AddChild(Row);
				}
			}
		}
		else
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Ranking] %s: RowClass is not set."), *GetName());
		}
	}

	if (Text_Empty)
	{
		Text_Empty->SetText(EmptyText);
		WarriorRankingView::SetShown(Text_Empty, Entries.IsEmpty());
	}

	if (Row_Mine)
	{
		const FWarriorRankingMine& Mine = Ranking->GetMine();
		if (Mine.bHasRecord)
		{
			Row_Mine->SetEntry(Mine.Entry, ViewKey.Kind);
		}
		else
		{
			Row_Mine->SetNoRecord(NoRecordText);
		}
	}

	if (Button_More)
	{
		Button_More->SetVisibility(bAllowMore && Ranking->HasMore() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	BP_OnViewStateSet(false, false, Entries.IsEmpty());
}

void UWarriorRankingViewWidget::SetLoading(bool bInLoading)
{
	bLoading = bInLoading;
	WarriorRankingView::SetShown(Widget_Loading, bLoading);
	if (bLoading)
	{
		BP_OnViewStateSet(true, false, false);
	}
}

void UWarriorRankingViewWidget::SetFailed(bool bInFailed)
{
	bFailed = bInFailed;
	WarriorRankingView::SetShown(Widget_Error, bFailed);
	if (Widget_Content)
	{
		WarriorRankingView::SetShown(Widget_Content, !bFailed);
	}
	else
	{
		WarriorRankingView::SetShown(Panel_Rows, !bFailed);
		WarriorRankingView::SetShown(Row_Mine, !bFailed);
	}
	if (bFailed)
	{
		WarriorRankingView::SetShown(Button_More, false);
		BP_OnViewStateSet(false, true, false);
	}
}

void UWarriorRankingViewWidget::UpdateRefreshButtons(bool bInForce)
{
	const int32 Remaining = Cooldown.RemainingSeconds(FPlatformTime::Seconds());
	const int32 Shown = bLoading ? -2 : Remaining;
	if (!bInForce && Shown == LastShownRemaining)
	{
		return;
	}
	LastShownRemaining = Shown;

	const bool bReady = !bLoading && Remaining == 0;
	auto Apply = [&](UButton* InButton, UTextBlock* InText, const FText& InBase)
	{
		if (InButton)
		{
			InButton->SetIsEnabled(bReady);
			//막힌 동안은 채도를 빼고 어둡게(도안 2.1판). 풀리면 원래 스타일
			InButton->SetColorAndOpacity(bReady ? FLinearColor::White : InactiveButtonColor);
		}
		if (InText)
		{
			InText->SetText(bLoading ? LoadingRefreshText
				: bReady ? InBase
				: FText::Format(CooldownFormat, InBase, FText::AsNumber(Remaining)));
		}
	};
	Apply(Button_Refresh, Text_Refresh, RefreshText);
	Apply(Button_Retry, Text_Retry, RetryText);

	BP_OnRefreshStateChanged(bReady, Remaining);
}

//──────────────────────────────── 메인메뉴 랭킹 팝업

UWarriorRankingPopupWidget::UWarriorRankingPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TotalTitle = LOCTEXT("TotalTitle", "랭킹 · 전체 총합");
	RegionTitle = LOCTEXT("RegionTitle", "랭킹 · 지역 총합");
	//전체 총합 탭은 설명 문구 없음(10/12 Sang 결정). 비어 있으면 Text_Meta를 접는다
	TotalMeta = FText::GetEmpty();
	RegionMeta = LOCTEXT("RegionMeta", "지역 총합 = 그 지역 스테이지마다 내 최고 점수의 합 · 같으면 먼저 달성한 사람이 위");
	DifficultyLabelFormat = LOCTEXT("DifficultyLabelFormat", "난이도: {0}");
	//"더 보기" 버튼 대신 스크롤 자동 받기
	bAllowMore = false;
}

void UWarriorRankingPopupWidget::RebuildRows(bool bInFirstPage)
{
	if (!ListView_Rows)
	{
		Super::RebuildRows(bInFirstPage);
		return;
	}

	const UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this);
	if (!Ranking)
	{
		return;
	}

	//받은 줄은 모두 메모리(항목 객체)에 남긴다. 위로 다시 스크롤해도 다시 받지 않는다(Sang 10/12).
	//줄 위젯은 ListView가 보이는 만큼만 만들어 재사용한다
	const TArray<FWarriorRankingEntry>& Entries = Ranking->GetEntries();
	if (bInFirstPage || ListView_Rows->GetNumItems() > Entries.Num())
	{
		ListView_Rows->ClearListItems();
	}
	for (int32 Index = ListView_Rows->GetNumItems(); Index < Entries.Num(); ++Index)
	{
		UWarriorRankingEntryItem* Item = NewObject<UWarriorRankingEntryItem>(this);
		Item->Entry = Entries[Index];
		Item->Kind = ViewKey.Kind;
		ListView_Rows->AddItem(Item);
	}

	//빈 목록 문구·내 줄·더 보기 버튼(없음)은 공통 처리를 쓴다(Panel_Rows가 없으면 줄은 건드리지 않음)
	if (Text_Empty)
	{
		Text_Empty->SetText(EmptyText);
		WarriorRankingView::SetShown(Text_Empty, Entries.IsEmpty());
	}
	if (Row_Mine)
	{
		const FWarriorRankingMine& Mine = Ranking->GetMine();
		if (Mine.bHasRecord)
		{
			Row_Mine->SetEntry(Mine.Entry, ViewKey.Kind);
		}
		else
		{
			Row_Mine->SetNoRecord(NoRecordText);
		}
	}
	WarriorRankingView::SetShown(Button_More, false);
	BP_OnViewStateSet(false, false, Entries.IsEmpty());
}

void UWarriorRankingPopupWidget::HandleListScrolled(float InItemOffset, float InDistanceRemaining)
{
	if (InDistanceRemaining <= LoadMoreThreshold)
	{
		TryLoadMore();
	}
}

void UWarriorRankingPopupWidget::TryLoadMore()
{
	UWarriorRankingSubsystem* Ranking = UWarriorRankingSubsystem::Get(this);
	if (!Ranking || !IsMyBoard() || bLoading || bFailed || !Ranking->HasMore() || Ranking->IsRequestingBoard())
	{
		return;
	}
	if (Ranking->GetEntries().Num() >= MaxLoadedRows)
	{
		return;
	}

	//빠른 스크롤에도 최소 간격보다 자주 보내지 않는다. 한 번에 한 요청은 서브시스템이 막는다
	const double Now = FPlatformTime::Seconds();
	if (Now < AppendBlockedUntilSeconds || Now - LastLoadMoreSeconds < LoadMoreMinInterval)
	{
		return;
	}
	LastLoadMoreSeconds = Now;
	Ranking->RequestMore();
}

void UWarriorRankingPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_TabTotal)
	{
		Button_TabTotal->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTotalTabClicked);
	}
	if (Button_TabRegion)
	{
		Button_TabRegion->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRegionTabClicked);
	}
	if (Button_PeriodAll)
	{
		Button_PeriodAll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAllClicked);
	}
	if (Button_PeriodWeekly)
	{
		Button_PeriodWeekly->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleWeeklyClicked);
	}
	if (ComboBox_Region)
	{
		ComboBox_Region->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleRegionSelected);
	}
	if (Button_Difficulty)
	{
		Button_Difficulty->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDifficultyClicked);
	}
	if (ListView_Rows)
	{
		ListView_Rows->OnListViewScrolled().AddUObject(this, &ThisClass::HandleListScrolled);
	}
}

void UWarriorRankingPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//지역 이름은 스테이지 목록(같은 계정, ST1 정의)에서 읽는다
	if (UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		StageList->OnStageListUpdated.AddUniqueDynamic(this, &ThisClass::HandleStageListUpdated);
	}
}

void UWarriorRankingPopupWidget::NativeDestruct()
{
	if (UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		StageList->OnStageListUpdated.RemoveDynamic(this, &ThisClass::HandleStageListUpdated);
	}

	Super::NativeDestruct();
}

void UWarriorRankingPopupWidget::Open()
{
	bTotalTab = true;
	Period = EWarriorRankingPeriod::All;

	UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (StageList && !StageList->HasStageList() && !StageList->IsRequesting())
	{
		StageList->RequestStageList();
	}
	//난이도는 스테이지 선택에서 고른 값부터(방금 노말로 깼으면 노말 보드가 먼저 보이게)
	const UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this);
	SetDifficulty(LaunchSubsystem ? LaunchSubsystem->GetSelectedDifficulty() : 0);
	FillRegions();
	RequestCurrent();
}

void UWarriorRankingPopupWidget::HandleDifficultyClicked()
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const int32 Count = StageList ? StageList->GetDifficulties().Num() : 0;
	if (Count <= 1)
	{
		return;
	}
	//화면에서 고른 난이도는 랭킹 팝업 안에서만 쓴다(스테이지 입장 난이도는 바꾸지 않음)
	SetDifficulty((Difficulty + 1) % Count);
	RequestCurrent();
}

void UWarriorRankingPopupWidget::SetDifficulty(int32 InDifficulty)
{
	//서버 목록(ST1 difficulties)에 있는 번호만. 목록을 아직 받는 중이면 그대로 두고 받은 뒤(HandleStageListUpdated) 다시 맞춘다.
	//목록에 없는 번호(M9 전 서버 포함)면 0
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (StageList && StageList->HasStageList())
	{
		Difficulty = StageList->HasDifficulty(InDifficulty) ? InDifficulty : 0;
	}
	else
	{
		Difficulty = FMath::Max(0, InDifficulty);
	}
	UpdateDifficultyLabel();
}

void UWarriorRankingPopupWidget::UpdateDifficultyLabel()
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const FText DisplayName = StageList ? StageList->GetDifficultyDisplayName(Difficulty) : FText::AsNumber(Difficulty);
	WarriorRankingView::SetText(Text_Difficulty, FText::Format(DifficultyLabelFormat, DisplayName));
	BP_OnDifficultySet(Difficulty, DisplayName);
}

void UWarriorRankingPopupWidget::HandleTotalTabClicked()
{
	if (!bTotalTab)
	{
		bTotalTab = true;
		RequestCurrent();
	}
}

void UWarriorRankingPopupWidget::HandleRegionTabClicked()
{
	if (bTotalTab)
	{
		bTotalTab = false;
		RequestCurrent();
	}
}

void UWarriorRankingPopupWidget::HandleAllClicked()
{
	if (Period != EWarriorRankingPeriod::All)
	{
		Period = EWarriorRankingPeriod::All;
		RequestCurrent();
	}
}

void UWarriorRankingPopupWidget::HandleWeeklyClicked()
{
	if (Period != EWarriorRankingPeriod::Weekly)
	{
		Period = EWarriorRankingPeriod::Weekly;
		RequestCurrent();
	}
}

void UWarriorRankingPopupWidget::HandleRegionSelected(FString InSelectedItem, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::Direct || !ComboBox_Region)
	{
		return;
	}
	const int32 Index = ComboBox_Region->GetSelectedIndex();
	if (RegionIds.IsValidIndex(Index) && RegionIds[Index] != SelectedRegionId)
	{
		SelectedRegionId = RegionIds[Index];
		if (!bTotalTab)
		{
			RequestCurrent();
		}
	}
}

void UWarriorRankingPopupWidget::HandleStageListUpdated()
{
	const bool bHadRegion = !SelectedRegionId.IsNone();
	FillRegions();
	//팝업을 열 때 목록을 받는 중이었으면 이제 난이도 이름이 생긴다. 고른 번호가 목록에 없으면 0으로(그 보드를 다시 받음)
	const int32 PreviousDifficulty = Difficulty;
	SetDifficulty(PreviousDifficulty);
	if (Difficulty != PreviousDifficulty && bHasView)
	{
		RequestCurrent();
		return;
	}
	//지역 총합 탭을 지역 없이 열었으면 지금 받는다
	if (!bTotalTab && !bHadRegion && !SelectedRegionId.IsNone())
	{
		RequestCurrent();
	}
}

void UWarriorRankingPopupWidget::FillRegions()
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (!StageList || !StageList->HasStageList())
	{
		return;
	}

	RegionIds.Reset();
	if (ComboBox_Region)
	{
		ComboBox_Region->ClearOptions();
	}
	int32 SelectedIndex = 0;
	for (const FWarriorRegionListEntry& Region : StageList->GetRegions())
	{
		if (Region.RegionId == SelectedRegionId)
		{
			SelectedIndex = RegionIds.Num();
		}
		RegionIds.Add(Region.RegionId);
		if (ComboBox_Region)
		{
			ComboBox_Region->AddOption(Region.DisplayName.ToString());
		}
	}
	if (RegionIds.IsValidIndex(SelectedIndex))
	{
		SelectedRegionId = RegionIds[SelectedIndex];
		if (ComboBox_Region)
		{
			ComboBox_Region->SetSelectedIndex(SelectedIndex);
		}
	}
}

void UWarriorRankingPopupWidget::RequestCurrent()
{
	WarriorRankingView::SetText(Text_Title, bTotalTab ? TotalTitle : RegionTitle);
	//설명 문구가 비어 있으면 줄째 접는다(전체 총합은 문구 없음)
	const FText& Meta = bTotalTab ? TotalMeta : RegionMeta;
	WarriorRankingView::SetText(Text_Meta, Meta);
	WarriorRankingView::SetShown(Text_Meta, !Meta.IsEmpty());
	if (ComboBox_Region)
	{
		//지역 총합 탭에서만 고르기를 보인다(지역이 하나여도 지금 어느 지역인지 보이게)
		ComboBox_Region->SetVisibility(!bTotalTab && RegionIds.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	BP_OnTabSet(bTotalTab, Period);

	if (bTotalTab)
	{
		RequestView(EWarriorRankingBoardKind::Total, NAME_None, Period, PageSize, Difficulty);
	}
	else if (!SelectedRegionId.IsNone())
	{
		RequestView(EWarriorRankingBoardKind::Region, SelectedRegionId, Period, PageSize, Difficulty);
	}
	else
	{
		//스테이지 목록을 받는 중. 받으면 HandleStageListUpdated가 다시 부른다
		SetLoading(true);
	}
}

//──────────────────────────────── 스테이지 순위 카드

UWarriorStageRankingCardWidget::UWarriorStageRankingCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TitleFormat = LOCTEXT("StageTitleFormat", "스테이지 {0} · {1} 순위 ({2})");
	EmptyText = LOCTEXT("StageEmptyText", "아직 아무도 클리어하지 않았습니다.");
	NoRecordText = LOCTEXT("StageNoRecordText", "기록 없음 — 클리어하면 순위에 올라갑니다.");
	bAllowMore = false;
}

void UWarriorStageRankingCardWidget::ShowStage(FName InStageId, const FText& InStageName, int32 InStageOrder, int32 InDifficulty)
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const FText DifficultyName = StageList ? StageList->GetDifficultyDisplayName(InDifficulty) : FText::AsNumber(InDifficulty);
	WarriorRankingView::SetText(Text_Title, FText::Format(TitleFormat, InStageOrder, InStageName, DifficultyName));
	//위 몇 명만(더 보기 없음), 전체 기간만(도안 2.1판 Q3), 스테이지 선택에서 고른 난이도의 보드(M9)
	RequestView(EWarriorRankingBoardKind::Stage, InStageId, EWarriorRankingPeriod::All, TopCount, InDifficulty);
	WarriorRankingView::SetShown(Button_More, false);
}

#undef LOCTEXT_NAMESPACE
