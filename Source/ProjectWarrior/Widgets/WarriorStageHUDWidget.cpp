// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageHUDWidget.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"

#define LOCTEXT_NAMESPACE "WarriorStageHUD"

namespace
{
	//보일 때는 HitTestInvisible로 둬서 HUD가 마우스 클릭을 가로채지 않게 한다.
	void SetHUDElementShown(UWidget* InWidget, bool bInShown)
	{
		if (InWidget)
		{
			InWidget->SetVisibility(bInShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

UWarriorStageHUDWidget::UWarriorStageHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//기본 문구. WBP 클래스 기본값에서 바꿀 수 있다.
	StateLabels.Add(EWarriorStageState::Preparing, LOCTEXT("StatePreparing", "준비"));
	StateLabels.Add(EWarriorStageState::InProgress, LOCTEXT("StateInProgress", "전투"));
	StateLabels.Add(EWarriorStageState::WaveCleared, LOCTEXT("StateWaveCleared", "웨이브 클리어"));
	StateLabels.Add(EWarriorStageState::Resting, LOCTEXT("StateResting", "쉬는 시간"));

	WaveFormat = LOCTEXT("WaveFormat", "웨이브 {0} / {1}");
	EnemyCountFormat = LOCTEXT("EnemyCountFormat", "남은 적 {0} / {1}");
	GoldDeltaFormat = LOCTEXT("GoldDeltaFormat", "+{0}");
	BossWaveLabel = LOCTEXT("BossWaveLabel", "보스 웨이브");
	WaveStartAnnounceFormat = LOCTEXT("WaveStartAnnounceFormat", "웨이브 {0}");
	BossWaveAnnounceText = LOCTEXT("BossWaveAnnounceText", "보스 웨이브");
	WaveClearedAnnounceText = LOCTEXT("WaveClearedAnnounceText", "웨이브 클리어");
}

void UWarriorStageHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bNormalWaveColorCached)
	{
		NormalWaveColor = Text_Wave->GetColorAndOpacity();
		bNormalWaveColorCached = true;
	}

	if (Text_BossWave)
	{
		Text_BossWave->SetText(BossWaveLabel);
	}
	StopAnnounce();
	StopGoldDelta();

	const UWorld* World = GetWorld();
	AWarriorStageGameState* StageGameState = World ? World->GetGameState<AWarriorStageGameState>() : nullptr;
	if (StageGameState)
	{
		BoundGameState = StageGameState;
		StageGameState->OnStageStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStageStateChanged);
		StageGameState->OnWaveChanged.AddUniqueDynamic(this, &ThisClass::HandleWaveChanged);
		StageGameState->OnEnemyCountChanged.AddUniqueDynamic(this, &ThisClass::HandleEnemyCountChanged);

		//구독 전에 바뀐 값을 놓치지 않도록 현재 값으로 한 번 채운다. 알림은 띄우지 않는다.
		RefreshWave(StageGameState->GetWaveNumber(), StageGameState->GetTotalWaveCount(), StageGameState->IsBossWave());
		RefreshEnemyCount(StageGameState->GetAliveEnemyCount(), StageGameState->GetTotalEnemyCount());
		RefreshState(StageGameState->GetStageState());
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s is used in a level without AWarriorStageGameState. HUD is hidden."), *GetName());
		SetVisibility(ESlateVisibility::Collapsed);
	}

	const AWarriorPlayerState* WarriorPlayerState = GetOwningPlayerState<AWarriorPlayerState>();
	UPlayerInventoryComponent* Inventory = WarriorPlayerState ? WarriorPlayerState->GetPlayerInventoryComponent() : nullptr;
	if (Inventory)
	{
		BoundInventory = Inventory;
		Inventory->OnGoldChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
		RefreshGold(Inventory->GetGold());
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: no player inventory. Gold stays at 0."), *GetName());
		RefreshGold(0);
	}
}

void UWarriorStageHUDWidget::NativeDestruct()
{
	if (AWarriorStageGameState* StageGameState = BoundGameState.Get())
	{
		StageGameState->OnStageStateChanged.RemoveDynamic(this, &ThisClass::HandleStageStateChanged);
		StageGameState->OnWaveChanged.RemoveDynamic(this, &ThisClass::HandleWaveChanged);
		StageGameState->OnEnemyCountChanged.RemoveDynamic(this, &ThisClass::HandleEnemyCountChanged);
	}
	BoundGameState.Reset();

	if (UPlayerInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}
	BoundInventory.Reset();

	Super::NativeDestruct();
}

void UWarriorStageHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateTimer();
	UpdateAnnounce(InDeltaTime);
	UpdateGoldDelta(InDeltaTime);
}

void UWarriorStageHUDWidget::HandleStageStateChanged(EWarriorStageState InNewState, EWarriorStageState InOldState)
{
	RefreshState(InNewState);

	if (InNewState == EWarriorStageState::WaveCleared)
	{
		ShowAnnounce(WaveClearedAnnounceText);
	}
}

void UWarriorStageHUDWidget::HandleWaveChanged(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave)
{
	RefreshWave(InWaveNumber, InTotalWaveCount, bInBossWave);

	//웨이브 0은 FlowManager 등록 때 전체 웨이브 수만 알리는 값이라 시작 알림을 띄우지 않는다.
	if (InWaveNumber > 0)
	{
		ShowAnnounce(bInBossWave ? BossWaveAnnounceText : FText::Format(WaveStartAnnounceFormat, InWaveNumber));
	}
}

void UWarriorStageHUDWidget::HandleEnemyCountChanged(int32 InAliveCount, int32 InTotalCount)
{
	RefreshEnemyCount(InAliveCount, InTotalCount);
}

void UWarriorStageHUDWidget::HandleGoldChanged(int32 InNewGold)
{
	const int32 Delta = InNewGold - LastGold;
	RefreshGold(InNewGold);

	//상점 구매처럼 줄어드는 경우에는 연출하지 않는다.
	if (Delta > 0)
	{
		PlayGoldDelta(Delta);
	}
}

bool UWarriorStageHUDWidget::IsHUDShownInState(EWarriorStageState InState)
{
	switch (InState)
	{
	case EWarriorStageState::Preparing:
	case EWarriorStageState::InProgress:
	case EWarriorStageState::WaveCleared:
	case EWarriorStageState::Resting:
		return true;
	default:
		return false;
	}
}

void UWarriorStageHUDWidget::RefreshState(EWarriorStageState InState)
{
	const bool bShowHUD = IsHUDShownInState(InState);
	SetVisibility(bShowHUD ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShowHUD)
	{
		//숨겨진 동안에는 Tick이 돌지 않으므로, 다시 보일 때 연출이 중간부터 이어지지 않게 끝내 둔다.
		StopAnnounce();
		StopGoldDelta();
		return;
	}

	Text_State->SetText(StateLabels.FindRef(InState));

	//적 수는 전투 중에만 의미가 있다.
	SetHUDElementShown(Text_EnemyCount, InState == EWarriorStageState::InProgress);

	//상태가 바뀌면 남은 시간이 새로 시작하므로 바로 다시 그린다.
	DisplayedTimerSeconds = INDEX_NONE;
	UpdateTimer();
}

void UWarriorStageHUDWidget::RefreshWave(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave)
{
	Text_Wave->SetText(FText::Format(WaveFormat, InWaveNumber, InTotalWaveCount));
	Text_Wave->SetColorAndOpacity(bInBossWave ? FSlateColor(BossWaveColor) : NormalWaveColor);

	SetHUDElementShown(Text_BossWave, bInBossWave);
}

void UWarriorStageHUDWidget::RefreshEnemyCount(int32 InAliveCount, int32 InTotalCount)
{
	Text_EnemyCount->SetText(FText::Format(EnemyCountFormat, InAliveCount, InTotalCount));
}

void UWarriorStageHUDWidget::RefreshGold(int32 InGold)
{
	LastGold = InGold;
	Text_Gold->SetText(FText::AsNumber(InGold));
}

void UWarriorStageHUDWidget::UpdateTimer()
{
	const AWarriorStageGameState* StageGameState = BoundGameState.Get();
	const float RemainingTime = StageGameState ? StageGameState->GetStateRemainingTime() : 0.f;
	const int32 RemainingSeconds = RemainingTime > 0.f ? FMath::CeilToInt(RemainingTime) : 0;
	if (RemainingSeconds == DisplayedTimerSeconds)
	{
		return;
	}

	DisplayedTimerSeconds = RemainingSeconds;

	//타이머가 없는 상태(전투)는 0이므로 숨긴다.
	SetHUDElementShown(Text_Timer, RemainingSeconds > 0);
	if (RemainingSeconds > 0)
	{
		Text_Timer->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60)));
	}
}

void UWarriorStageHUDWidget::ShowAnnounce(const FText& InText)
{
	if (!Text_Announce)
	{
		return;
	}

	Text_Announce->SetText(InText);
	Text_Announce->SetRenderOpacity(1.f);
	SetHUDElementShown(Text_Announce, true);

	bAnnounceActive = true;
	AnnounceElapsed = 0.f;
}

void UWarriorStageHUDWidget::UpdateAnnounce(float InDeltaTime)
{
	if (!bAnnounceActive || !Text_Announce)
	{
		return;
	}

	AnnounceElapsed += InDeltaTime;
	if (AnnounceElapsed >= AnnounceDuration)
	{
		StopAnnounce();
		return;
	}

	//끝나기 전 AnnounceFadeTime 동안만 투명해진다.
	const float TimeLeft = AnnounceDuration - AnnounceElapsed;
	if (AnnounceFadeTime > 0.f && TimeLeft < AnnounceFadeTime)
	{
		Text_Announce->SetRenderOpacity(TimeLeft / AnnounceFadeTime);
	}
}

void UWarriorStageHUDWidget::StopAnnounce()
{
	bAnnounceActive = false;
	SetHUDElementShown(Text_Announce, false);
}

void UWarriorStageHUDWidget::PlayGoldDelta(int32 InDelta)
{
	if (!Text_GoldDelta)
	{
		return;
	}

	GoldDeltaAmount = bGoldDeltaActive ? GoldDeltaAmount + InDelta : InDelta;
	bGoldDeltaActive = true;
	GoldDeltaElapsed = 0.f;

	Text_GoldDelta->SetText(FText::Format(GoldDeltaFormat, GoldDeltaAmount));
	Text_GoldDelta->SetRenderTranslation(FVector2D(0.f, GoldDeltaRiseDistance));
	Text_GoldDelta->SetRenderOpacity(1.f);
	SetHUDElementShown(Text_GoldDelta, true);
}

void UWarriorStageHUDWidget::UpdateGoldDelta(float InDeltaTime)
{
	if (!bGoldDeltaActive || !Text_GoldDelta)
	{
		return;
	}

	GoldDeltaElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(GoldDeltaElapsed / GoldDeltaDuration, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		StopGoldDelta();
		return;
	}

	//아래에서 골드 쪽(배치한 자리)으로 올라가며, 끝으로 갈수록 빠르게 투명해진다.
	const float MoveAlpha = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
	Text_GoldDelta->SetRenderTranslation(FVector2D(0.f, GoldDeltaRiseDistance * (1.f - MoveAlpha)));
	Text_GoldDelta->SetRenderOpacity(1.f - Alpha * Alpha);
}

void UWarriorStageHUDWidget::StopGoldDelta()
{
	bGoldDeltaActive = false;
	GoldDeltaAmount = 0;

	if (Text_GoldDelta)
	{
		Text_GoldDelta->SetRenderTranslation(FVector2D::ZeroVector);
	}
	SetHUDElementShown(Text_GoldDelta, false);
}

#undef LOCTEXT_NAMESPACE
