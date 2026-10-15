// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageSelectWidgets.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"
#include "ProjectWarrior/Network/WarriorStageListSubsystem.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"
#include "ProjectWarrior/Widgets/WarriorRankingWidgets.h"

#define LOCTEXT_NAMESPACE "WarriorStageSelect"

//──────────────────────────────── 웨이브 줄

UWarriorWaveEntryWidget::UWarriorWaveEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaveLabelFormat = LOCTEXT("WaveLabelFormat", "웨이브 {0}");
	EnemyFormat = LOCTEXT("EnemyFormat", "{0} × {1}");
	BossSuffix = LOCTEXT("BossSuffix", " (보스)");
}

void UWarriorWaveEntryWidget::SetWave(const FWarriorStageWaveInfo& InWave, int32 InDifficulty)
{
	if (Text_WaveLabel)
	{
		Text_WaveLabel->SetText(FText::Format(WaveLabelFormat, InWave.WaveNumber));
	}

	if (Text_Enemies)
	{
		TArray<FText> Parts;
		for (const FWarriorStageWaveEnemyInfo& Enemy : InWave.Enemies)
		{
			const FText Part = FText::Format(EnemyFormat, Enemy.DisplayName, Enemy.GetCount(InDifficulty));
			Parts.Add(Enemy.bBoss ? FText::Format(LOCTEXT("BossJoin", "{0}{1}"), Part, BossSuffix) : Part);
		}
		Text_Enemies->SetText(FText::Join(LOCTEXT("EnemySeparator", " · "), Parts));
	}
}

//──────────────────────────────── 스테이지 막대

UWarriorStageEntryWidget::UWarriorStageEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StageLabelFormat = LOCTEXT("StageLabelFormat", "STAGE {0}");
	WaveCountFormat = LOCTEXT("WaveCountFormat", "웨이브 {0}");
	ClearedInfoFormat = LOCTEXT("ClearedInfoFormat", "웨이브 {0} · 처음 클리어 {1}");

	//메인메뉴 StageLabelText 색(기존 값), 클리어는 금색(제안 값, 팀 회의에서 바꿀 수 있음), 잠김은 비활성 테두리 색
	OpenLabelColor = FSlateColor(FLinearColor(0.871f, 0.301f, 0.198f, 1.f));
	ClearedLabelColor = FSlateColor(FLinearColor(0.58f, 0.36f, 0.10f, 1.f));
	LockedLabelColor = FSlateColor(FLinearColor(0.456f, 0.445f, 0.397f, 1.f));
}

void UWarriorStageEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Bar)
	{
		Button_Bar->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBarClicked);
	}

	if (Button_Enter)
	{
		Button_Enter->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEnterClicked);
	}

	if (Button_Ranking)
	{
		Button_Ranking->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRankingClicked);
	}
}

void UWarriorStageEntryWidget::SetStage(const FWarriorStageListEntry& InStage, int32 InDifficulty)
{
	const bool bSameStage = Stage.StageId == InStage.StageId;
	Stage = InStage;
	Difficulty = InDifficulty;

	if (Text_StageLabel)
	{
		Text_StageLabel->SetText(FText::Format(StageLabelFormat, Stage.Order));
	}

	if (Text_StageName)
	{
		Text_StageName->SetText(Stage.DisplayName);
	}

	if (Text_StageInfo)
	{
		if (Stage.bHasFirstCleared)
		{
			//서버 시각은 UTC. 날짜는 이 PC의 시간대로 보인다
			const FDateTime LocalTime = Stage.FirstClearedAt + (FDateTime::Now() - FDateTime::UtcNow());
			const FText DateText = FText::FromString(LocalTime.ToString(TEXT("%m/%d")));
			Text_StageInfo->SetText(FText::Format(ClearedInfoFormat, Stage.WaveCount, DateText));
		}
		else
		{
			Text_StageInfo->SetText(FText::Format(WaveCountFormat, Stage.WaveCount));
		}
	}

	//목록을 다시 받으면 웨이브 줄도 다시 만든다. 같은 스테이지면 펼친 상태를 유지한다
	bWavesBuilt = false;
	SetWavesExpanded(bSameStage && bWavesExpanded && Stage.bCanEnter);

	ApplyStatusLook();
	BP_OnStageStatusSet(Stage.Status);
}

void UWarriorStageEntryWidget::SetInteractionLocked(bool bInLocked)
{
	bInteractionLocked = bInLocked;
	ApplyStatusLook();
}

void UWarriorStageEntryWidget::ApplyStatusLook()
{
	const bool bLocked = Stage.Status == EWarriorStageEntryStatus::Locked;

	//잠긴 막대는 눌러도 반응이 없다(웨이브 정보도 열지 않음, U-9). 비활성 버튼 모양은 WBP의 Disabled 스타일
	if (Button_Bar)
	{
		Button_Bar->SetIsEnabled(!bLocked && !bInteractionLocked);
	}

	if (Button_Enter)
	{
		Button_Enter->SetIsEnabled(Stage.bCanEnter && !bInteractionLocked);
		Button_Enter->SetVisibility(Stage.bCanEnter ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	//순위는 열린·클리어 스테이지만(잠긴 스테이지는 숨김, 도안 2.1판). 입장 요청 중에도 볼 수 있다
	if (Button_Ranking)
	{
		Button_Ranking->SetVisibility(bLocked ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}

	if (Text_StageLabel)
	{
		Text_StageLabel->SetColorAndOpacity(bLocked ? LockedLabelColor
			: (Stage.Status == EWarriorStageEntryStatus::Cleared ? ClearedLabelColor : OpenLabelColor));
	}

	SetRenderOpacity(bLocked ? LockedOpacity : 1.f);
}

void UWarriorStageEntryWidget::HandleBarClicked()
{
	if (!Stage.bCanEnter || bInteractionLocked)
	{
		return;
	}

	SetWavesExpanded(!bWavesExpanded);
}

void UWarriorStageEntryWidget::HandleEnterClicked()
{
	if (!Stage.bCanEnter || bInteractionLocked)
	{
		return;
	}

	OnEnterClicked.Broadcast(Stage.StageId);
}

void UWarriorStageEntryWidget::HandleRankingClicked()
{
	if (Stage.Status == EWarriorStageEntryStatus::Locked)
	{
		return;
	}

	OnRankingClicked.Broadcast(Stage.StageId);
}

void UWarriorStageEntryWidget::SetDifficulty(int32 InDifficulty)
{
	if (Difficulty == InDifficulty)
	{
		return;
	}

	Difficulty = InDifficulty;
	bWavesBuilt = false;
	SetWavesExpanded(bWavesExpanded);
}

void UWarriorStageEntryWidget::SetWavesExpanded(bool bInExpanded)
{
	bWavesExpanded = bInExpanded;

	if (!ScrollBox_Waves)
	{
		return;
	}

	if (bWavesExpanded && !bWavesBuilt)
	{
		ScrollBox_Waves->ClearChildren();
		if (WaveEntryClass)
		{
			for (const FWarriorStageWaveInfo& Wave : Stage.Waves)
			{
				if (UWarriorWaveEntryWidget* WaveEntry = CreateWidget<UWarriorWaveEntryWidget>(this, WaveEntryClass))
				{
					WaveEntry->SetWave(Wave, Difficulty);
					ScrollBox_Waves->AddChild(WaveEntry);
				}
			}
		}
		else
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[StageSelect] %s: WaveEntryClass is not set. Wave info is not shown."), *GetName());
		}
		bWavesBuilt = true;
	}

	ScrollBox_Waves->SetVisibility(bWavesExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

//──────────────────────────────── 지역 칸

void UWarriorRegionEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Region)
	{
		Button_Region->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRegionClicked);
	}
}

void UWarriorRegionEntryWidget::SetRegion(const FWarriorRegionListEntry& InRegion, bool bInSelected)
{
	RegionId = InRegion.RegionId;
	bCanOpen = InRegion.bCanOpen;

	if (Text_RegionName)
	{
		Text_RegionName->SetText(InRegion.DisplayName);
	}

	if (Button_Region)
	{
		Button_Region->SetIsEnabled(bCanOpen);
	}

	RebuildPips(InRegion);
	BP_OnRegionStateSet(bInSelected, bCanOpen);
}

void UWarriorRegionEntryWidget::RebuildPips(const FWarriorRegionListEntry& InRegion)
{
	if (!HorizontalBox_Pips)
	{
		return;
	}

	HorizontalBox_Pips->ClearChildren();
	for (int32 Index = 0; Index < InRegion.Stages.Num(); ++Index)
	{
		UImage* Pip = NewObject<UImage>(this);
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.ImageSize = PipSize;
		Brush.TintColor = FSlateColor(Index < InRegion.ClearedCount ? PipClearedColor : PipEmptyColor);
		Pip->SetBrush(Brush);

		if (UHorizontalBoxSlot* PipSlot = HorizontalBox_Pips->AddChildToHorizontalBox(Pip))
		{
			PipSlot->SetPadding(FMargin(0.f, 0.f, PipSpacing, 0.f));
			PipSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UWarriorRegionEntryWidget::HandleRegionClicked()
{
	if (bCanOpen)
	{
		OnRegionClicked.Broadcast(RegionId);
	}
}

//──────────────────────────────── 팝업

UWarriorStageSelectPopupWidget::UWarriorStageSelectPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	DifficultyLabelFormat = LOCTEXT("DifficultyLabelFormat", "난이도: {0}");
}

void UWarriorStageSelectPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (Button_Retry)
	{
		Button_Retry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	}

	if (Button_Difficulty)
	{
		Button_Difficulty->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDifficultyClicked);
	}
}

void UWarriorStageSelectPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//팝업은 다시 띄울 때 재사용하므로 화면에 있는 동안만 알림을 받는다
	if (UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		StageList->OnStageListUpdated.AddUniqueDynamic(this, &ThisClass::HandleListUpdated);
		StageList->OnStageListFailed.AddUniqueDynamic(this, &ThisClass::HandleListFailed);
	}
}

void UWarriorStageSelectPopupWidget::NativeDestruct()
{
	if (UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		StageList->OnStageListUpdated.RemoveDynamic(this, &ThisClass::HandleListUpdated);
		StageList->OnStageListFailed.RemoveDynamic(this, &ThisClass::HandleListFailed);
	}

	Super::NativeDestruct();
}

void UWarriorStageSelectPopupWidget::Refresh()
{
	bEnterPending = false;

	UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (!StageList)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StageSelect] No stage list subsystem."));
		SetViewState(EWarriorStageSelectViewState::Error);
		return;
	}

	//이전 목록이 있으면 받는 동안 그대로 보여 주고, 처음이면 받는 중 표시
	if (StageList->HasStageList())
	{
		HandleListUpdated();
	}
	else
	{
		SetViewState(EWarriorStageSelectViewState::Loading);
	}

	StageList->RequestStageList();
}

void UWarriorStageSelectPopupWidget::Close()
{
	//순위 카드가 떠 있으면 같이 닫는다(카드는 이 팝업 위 한 겹)
	if (StageRankingCard && StageRankingCard->IsInViewport())
	{
		StageRankingCard->RemoveFromParent();
	}
	RemoveFromParent();
	OnClosed.Broadcast();
}

void UWarriorStageSelectPopupWidget::HandleStageRankingClicked(FName InStageId)
{
	if (!StageRankingCardClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StageSelect] %s: StageRankingCardClass is not set."), *GetName());
		return;
	}

	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const FWarriorStageListEntry* Stage = StageList ? StageList->FindStage(InStageId) : nullptr;
	if (!Stage)
	{
		return;
	}

	if (!StageRankingCard)
	{
		StageRankingCard = CreateWidget<UWarriorStageRankingCardWidget>(GetOwningPlayer(), StageRankingCardClass);
		if (!StageRankingCard)
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[StageSelect] Failed to create stage ranking card."));
			return;
		}
		StageRankingCard->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleStageRankingCardClosed);
	}

	if (!StageRankingCard->IsInViewport())
	{
		StageRankingCard->AddToViewport(StageRankingCardZOrder);
	}
	StageRankingCard->ShowStage(Stage->StageId, Stage->DisplayName, Stage->Order, GetDifficulty());
	StageRankingCard->SetKeyboardFocus();
}

void UWarriorStageSelectPopupWidget::HandleStageRankingCardClosed()
{
	//카드를 닫으면 스테이지 선택으로 입력을 돌려준다
	SetKeyboardFocus();
}

void UWarriorStageSelectPopupWidget::HandleEnterFailed()
{
	bEnterPending = false;
	for (UWarriorStageEntryWidget* StageEntry : StageEntries)
	{
		if (StageEntry)
		{
			StageEntry->SetInteractionLocked(false);
		}
	}

	//실패 이유가 "그사이 상태가 바뀜"일 수 있으므로 목록을 다시 받는다
	if (UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		StageList->RequestStageList();
	}
}

void UWarriorStageSelectPopupWidget::HandleListUpdated()
{
	SetViewState(EWarriorStageSelectViewState::List);
	//서버 난이도 목록이 바뀌었을 수 있다(없는 번호면 0으로)
	SetDifficulty(GetDifficulty());
	RebuildRegions();
	RebuildStages();
}

int32 UWarriorStageSelectPopupWidget::GetDifficulty() const
{
	const UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this);
	return LaunchSubsystem ? LaunchSubsystem->GetSelectedDifficulty() : 0;
}

void UWarriorStageSelectPopupWidget::SetDifficulty(int32 InDifficulty)
{
	//서버 목록(ST1 difficulties)에 있는 번호만. 목록이 없으면(M9 전 서버) 0만
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const bool bKnown = StageList && StageList->GetDifficulties().ContainsByPredicate(
		[InDifficulty](const FWarriorDifficultyInfo& Info) { return Info.Difficulty == InDifficulty; });
	const int32 NewDifficulty = bKnown ? InDifficulty : 0;

	if (UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(this))
	{
		LaunchSubsystem->SetSelectedDifficulty(NewDifficulty);
	}

	for (UWarriorStageEntryWidget* StageEntry : StageEntries)
	{
		if (StageEntry)
		{
			StageEntry->SetDifficulty(NewDifficulty);
		}
	}
	UpdateDifficultyLabel();
}

void UWarriorStageSelectPopupWidget::HandleDifficultyClicked()
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	const int32 Count = StageList ? StageList->GetDifficulties().Num() : 0;
	if (bEnterPending || Count <= 1)
	{
		return;
	}
	SetDifficulty((GetDifficulty() + 1) % Count);
}

void UWarriorStageSelectPopupWidget::UpdateDifficultyLabel()
{
	const int32 Difficulty = GetDifficulty();
	FText DisplayName = FText::AsNumber(Difficulty);
	if (const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this))
	{
		if (const FWarriorDifficultyInfo* Info = StageList->GetDifficulties().FindByPredicate(
			[Difficulty](const FWarriorDifficultyInfo& Entry) { return Entry.Difficulty == Difficulty; }))
		{
			DisplayName = Info->DisplayName;
		}
	}

	if (Text_Difficulty)
	{
		Text_Difficulty->SetText(FText::Format(DifficultyLabelFormat, DisplayName));
	}
	BP_OnDifficultyChanged(Difficulty, DisplayName);
}

void UWarriorStageSelectPopupWidget::HandleListFailed(const FString& InErrorCode, const FString& InErrorMessage)
{
	//이미 보여 주던 목록이 있으면 그대로 둔다(로그는 서브시스템이 남김)
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (!StageList || !StageList->HasStageList())
	{
		SetViewState(EWarriorStageSelectViewState::Error);
	}
}

void UWarriorStageSelectPopupWidget::HandleCloseClicked()
{
	if (!bEnterPending)
	{
		Close();
	}
}

void UWarriorStageSelectPopupWidget::HandleRetryClicked()
{
	Refresh();
}

void UWarriorStageSelectPopupWidget::HandleRegionClicked(FName InRegionId)
{
	if (SelectedRegionId == InRegionId)
	{
		return;
	}

	SelectedRegionId = InRegionId;
	RebuildRegions();
	RebuildStages();
}

void UWarriorStageSelectPopupWidget::HandleStageEnterClicked(FName InStageId)
{
	if (bEnterPending)
	{
		return;
	}

	AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>();
	if (!FrontPlayerController)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StageSelect] %s: owning player is not AWarriorFrontPlayerController. Enter is not requested."), *GetName());
		return;
	}

	//두 번 누름 방지. 지금은 바로 레벨로 이동하지만, 서버 확인을 먼저 하게 되면(TD-30) 응답까지 막아 둔다
	bEnterPending = true;
	for (UWarriorStageEntryWidget* StageEntry : StageEntries)
	{
		if (StageEntry)
		{
			StageEntry->SetInteractionLocked(true);
		}
	}

	FrontPlayerController->EnterStage(InStageId);
}

void UWarriorStageSelectPopupWidget::SetViewState(EWarriorStageSelectViewState InState)
{
	if (Widget_Loading)
	{
		Widget_Loading->SetVisibility(InState == EWarriorStageSelectViewState::Loading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Widget_Error)
	{
		Widget_Error->SetVisibility(InState == EWarriorStageSelectViewState::Error ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ScrollBox_Stages)
	{
		ScrollBox_Stages->SetVisibility(InState == EWarriorStageSelectViewState::List ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UWarriorStageSelectPopupWidget::RebuildRegions()
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (!StageList || !HorizontalBox_Regions)
	{
		return;
	}

	const TArray<FWarriorRegionListEntry>& Regions = StageList->GetRegions();

	//고른 지역이 없거나 사라졌으면 첫 지역
	if (!Regions.ContainsByPredicate([this](const FWarriorRegionListEntry& Region) { return Region.RegionId == SelectedRegionId; }))
	{
		SelectedRegionId = Regions.Num() > 0 ? Regions[0].RegionId : NAME_None;
	}

	HorizontalBox_Regions->ClearChildren();
	if (!RegionEntryClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StageSelect] %s: RegionEntryClass is not set."), *GetName());
		return;
	}

	for (const FWarriorRegionListEntry& Region : Regions)
	{
		if (UWarriorRegionEntryWidget* RegionEntry = CreateWidget<UWarriorRegionEntryWidget>(this, RegionEntryClass))
		{
			RegionEntry->SetRegion(Region, Region.RegionId == SelectedRegionId);
			RegionEntry->OnRegionClicked.AddUObject(this, &ThisClass::HandleRegionClicked);
			HorizontalBox_Regions->AddChild(RegionEntry);
		}
	}
}

void UWarriorStageSelectPopupWidget::RebuildStages()
{
	const UWarriorStageListSubsystem* StageList = UWarriorStageListSubsystem::Get(this);
	if (!StageList || !ScrollBox_Stages)
	{
		return;
	}

	const FWarriorRegionListEntry* SelectedRegion = StageList->GetRegions().FindByPredicate(
		[this](const FWarriorRegionListEntry& Region) { return Region.RegionId == SelectedRegionId; });

	ScrollBox_Stages->ClearChildren();
	StageEntries.Reset();

	if (!SelectedRegion)
	{
		return;
	}

	if (!StageEntryClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StageSelect] %s: StageEntryClass is not set."), *GetName());
		return;
	}

	for (const FWarriorStageListEntry& Stage : SelectedRegion->Stages)
	{
		if (UWarriorStageEntryWidget* StageEntry = CreateWidget<UWarriorStageEntryWidget>(this, StageEntryClass))
		{
			StageEntry->SetStage(Stage, GetDifficulty());
			StageEntry->SetInteractionLocked(bEnterPending);
			StageEntry->OnEnterClicked.AddUObject(this, &ThisClass::HandleStageEnterClicked);
			StageEntry->OnRankingClicked.AddUObject(this, &ThisClass::HandleStageRankingClicked);
			ScrollBox_Stages->AddChild(StageEntry);
			StageEntries.Add(StageEntry);
		}
	}
}

#undef LOCTEXT_NAMESPACE
