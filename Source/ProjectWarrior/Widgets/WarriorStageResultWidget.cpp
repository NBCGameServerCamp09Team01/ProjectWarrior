// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageResultWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/Controllers/WarriorStagePlayerController.h"
#include "ProjectWarrior/Stats/WarriorProfileStatsSubsystem.h"
#include "ProjectWarrior/Stats/WarriorStageStatsSubsystem.h"

#define LOCTEXT_NAMESPACE "WarriorStageResult"

namespace
{
	//WBP에 없는 칸(BindWidgetOptional)은 건너뛴다.
	void SetTextIfBound(UTextBlock* InTextBlock, const FText& InText)
	{
		if (InTextBlock)
		{
			InTextBlock->SetText(InText);
		}
	}

	//초 단위 시간을 "m:ss"로. HUD 타이머와 같은 형식
	FText FormatMinutesSeconds(int32 InSeconds)
	{
		const int32 Seconds = FMath::Max(0, InSeconds);
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60));
	}
}

UWarriorStageResultWidget::UWarriorStageResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//UI 전용 입력 모드에서 이 위젯에 포커스를 주므로 켜 둔다. WBP에서 체크를 빠뜨려도 Non-Focusable 경고가 나지 않는다.
	SetIsFocusable(true);

	//기본 문구. WBP 클래스 기본값에서 바꿀 수 있다.
	ReachedWaveFormat = LOCTEXT("ReachedWaveFormat", "{0} / {1}");
	ExpGainedFormat = LOCTEXT("ExpGainedFormat", "+{0}");
	LevelUpFormat = LOCTEXT("LevelUpFormat", "Lv. {0} → {1}");
	LevelFormat = LOCTEXT("LevelFormat", "Lv. {0}");
	StatPointsGainedFormat = LOCTEXT("StatPointsGainedFormat", "+{0}");
	ExpProgressFormat = LOCTEXT("ExpProgressFormat", "{0} / {1}");
	MaxLevelText = LOCTEXT("MaxLevelText", "MAX");
	EmptyValueText = LOCTEXT("EmptyValueText", "-");
}

void UWarriorStageResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//값을 받기 전(또는 받지 못할 때) WBP의 예시 글자가 보이지 않도록 빈 값으로 채운다. 도달 웨이브·플레이 시간은 바로 뒤 SetResult가 채운다.
	for (UTextBlock* ValueText : { Text_ReachedWave.Get(), Text_PlayTime.Get(), Text_Kills.Get(), Text_GoldEarned.Get(),
		Text_ExpGained.Get(), Text_ExpProgress.Get(), Text_LevelChange.Get(), Text_StatPointsGained.Get() })
	{
		SetTextIfBound(ValueText, EmptyValueText);
	}

	if (ProgressBar_Exp)
	{
		ProgressBar_Exp->SetPercent(0.f);
	}

	//레벨 업 묶음은 이번 판에 레벨이 올랐다는 보상이 왔을 때만 보인다.
	if (LevelUpBox)
	{
		LevelUpBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	//이번 스테이지의 통계 기록 번호. 스테이지 통계가 스테이지 시작 때 만들고, 기록을 마감한 뒤에도 유지한다.
	const UWarriorStageStatsSubsystem* StageStats = UWarriorStageStatsSubsystem::Get(this);
	ExpectedRecordId = StageStats ? StageStats->GetCurrentStageRecord().RecordId : FGuid();
	if (!ExpectedRecordId.IsValid())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: no stage stats record in this level. Stats and reward are not shown."), *GetName());
		return;
	}

	//기록과 보상은 보통 이 뒤에 알림으로 온다. 알림 순서가 바뀌어 이미 와 있으면 마지막 값에서 읽는다(번호가 같을 때만 표시).
	if (UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(this))
	{
		ProfileStats->OnStageRecorded.AddUniqueDynamic(this, &ThisClass::HandleStageRecorded);

		FWarriorStageRecord LastStageRecord;
		if (ProfileStats->GetLastStageRecord(LastStageRecord))
		{
			HandleStageRecorded(LastStageRecord);
		}
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: profile stats subsystem is missing. Stats are not shown."), *GetName());
	}

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnStageRewarded.AddUniqueDynamic(this, &ThisClass::HandleStageRewarded);

		FWarriorStageReward LastReward;
		if (Account->GetLastStageReward(LastReward))
		{
			HandleStageRewarded(LastReward);
		}
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: account subsystem is missing. Reward is not shown."), *GetName());
	}
}

void UWarriorStageResultWidget::NativeDestruct()
{
	if (UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(this))
	{
		ProfileStats->OnStageRecorded.RemoveDynamic(this, &ThisClass::HandleStageRecorded);
	}

	if (UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Account->OnStageRewarded.RemoveDynamic(this, &ThisClass::HandleStageRewarded);
	}

	Super::NativeDestruct();
}

void UWarriorStageResultWidget::SetResult(const FWarriorStageResult& InResult)
{
	Result = InResult;

	SetTextIfBound(Text_ReachedWave, FText::Format(ReachedWaveFormat, Result.ReachedWave, Result.TotalWaveCount));
	//준비 시간 시작부터 결과까지의 실제 시간(시간 배율의 영향을 받지 않는다)
	SetTextIfBound(Text_PlayTime, FormatMinutesSeconds(FMath::FloorToInt32(Result.PlayTimeSeconds)));

	BP_OnResultSet(Result);
}

void UWarriorStageResultWidget::RequestRestart()
{
	if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
	{
		StagePlayerController->RestartStage();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: owning player is not AWarriorStagePlayerController. Restart is ignored."), *GetName());
}

void UWarriorStageResultWidget::RequestReturnToMainMenu()
{
	if (AWarriorStagePlayerController* StagePlayerController = GetOwningPlayer<AWarriorStagePlayerController>())
	{
		StagePlayerController->ReturnToMainMenu();
		return;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] %s: owning player is not AWarriorStagePlayerController. Return to main menu is ignored."), *GetName());
}

void UWarriorStageResultWidget::HandleStageRecorded(const FWarriorStageRecord& InStageRecord)
{
	//이전 판의 기록(GetLastStageRecord로 읽은 값)은 번호가 달라 여기서 걸러진다.
	if (!ExpectedRecordId.IsValid() || InStageRecord.RecordId != ExpectedRecordId)
	{
		return;
	}

	ApplyStageRecord(InStageRecord);
}

void UWarriorStageResultWidget::HandleStageRewarded(const FWarriorStageReward& InReward)
{
	if (!ExpectedRecordId.IsValid() || InReward.RecordId != ExpectedRecordId)
	{
		return;
	}

	ApplyStageReward(InReward);
}

void UWarriorStageResultWidget::ApplyStageRecord(const FWarriorStageRecord& InStageRecord)
{
	//플레이 시간은 통계(월드 시간)가 아니라 SetResult의 실제 시간을 쓴다.
	//번 골드는 이번 스테이지의 총 획득량이다(상점에서 쓴 골드를 빼지 않는다).
	const int32 Kills = InStageRecord.Stats.Attack.Kills;
	const int32 GoldEarned = InStageRecord.Stats.Economy.GoldEarned;

	SetTextIfBound(Text_Kills, FText::AsNumber(Kills));
	SetTextIfBound(Text_GoldEarned, FText::AsNumber(GoldEarned));

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Result stats applied. Record %s, Kills %d, GoldEarned %d"),
		*InStageRecord.RecordId.ToString(),
		Kills,
		GoldEarned);
}

void UWarriorStageResultWidget::ApplyStageReward(const FWarriorStageReward& InReward)
{
	SetTextIfBound(Text_ExpGained, FText::Format(ExpGainedFormat, InReward.ExpGained));
	SetTextIfBound(Text_LevelChange, InReward.DidLevelUp()
		? FText::Format(LevelUpFormat, InReward.LevelBefore, InReward.LevelAfter)
		: FText::Format(LevelFormat, InReward.LevelAfter));
	SetTextIfBound(Text_StatPointsGained, FText::Format(StatPointsGainedFormat, InReward.StatPointsGained));

	//스탯 포인트는 레벨 업 때만 생기므로 레벨 변화와 함께 묶어 레벨이 올랐을 때만 보인다.
	if (LevelUpBox)
	{
		LevelUpBox->SetVisibility(InReward.DidLevelUp() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	//경험치 진행도는 보상 구조에 없으므로 계정의 현재 값을 읽는다. 보상 알림 시점에는 계정 값이 이미 갱신되어 있다.
	int32 Experience = 0;
	int32 ExperienceToNextLevel = 0;
	if (const UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(this))
	{
		Experience = Account->GetExperience();
		ExperienceToNextLevel = Account->GetExperienceToNextLevel();

		//최대 레벨이면 다음 레벨이 없다(필요 경험치 0).
		const bool bMaxLevel = ExperienceToNextLevel <= 0;
		SetTextIfBound(Text_ExpProgress, bMaxLevel ? MaxLevelText : FText::Format(ExpProgressFormat, Experience, ExperienceToNextLevel));
		if (ProgressBar_Exp)
		{
			ProgressBar_Exp->SetPercent(bMaxLevel ? 1.f : FMath::Clamp(static_cast<float>(Experience) / ExperienceToNextLevel, 0.f, 1.f));
		}
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Result reward applied. Record %s, Exp +%d (now %d / %d), Level %d -> %d, StatPoints +%d"),
		*InReward.RecordId.ToString(),
		InReward.ExpGained,
		Experience,
		ExperienceToNextLevel,
		InReward.LevelBefore,
		InReward.LevelAfter,
		InReward.StatPointsGained);
}

#undef LOCTEXT_NAMESPACE
