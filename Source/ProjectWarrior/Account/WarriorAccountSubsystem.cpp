// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAccountSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/Stats/WarriorProfileStatsSubsystem.h"
#include "WarriorAccountTags.h"

namespace
{
	const FName ExpName(TEXT("Exp"));
	const FName StatPointName(TEXT("StatPoint"));
}

UWarriorAccountSubsystem* UWarriorAccountSubsystem::Get(const UObject* WorldContextObject)
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorAccountSubsystem>() : nullptr;
}

void UWarriorAccountSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//통계 서브시스템이 먼저 만들어져야 기록 완료 알림을 받을 수 있다.
	if (UWarriorProfileStatsSubsystem* Stats = Collection.InitializeDependency<UWarriorProfileStatsSubsystem>())
	{
		BoundStats = Stats;
		Stats->OnStageRecorded.AddUniqueDynamic(this, &ThisClass::HandleStageRecorded);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] Stats subsystem is missing. Stage rewards will not be granted."));
	}

	BuildDefinitions();

	//TODO(server): 서버 연동 뒤에는 로그인 직후 서버의 계정 데이터로 ApplyServerSnapshot을 호출해 초기화한다.
	Data = FWarriorAccountData();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Init. Level %d, StatPoints %d"), Data.AccountLevel, Data.StatPoints);
}

void UWarriorAccountSubsystem::Deinitialize()
{
	if (UWarriorProfileStatsSubsystem* Stats = BoundStats.Get())
	{
		Stats->OnStageRecorded.RemoveDynamic(this, &ThisClass::HandleStageRecorded);
	}
	BoundStats.Reset();

	Super::Deinitialize();
}

int32 UWarriorAccountSubsystem::GetExperienceToNextLevel() const
{
	if (Data.AccountLevel >= Rules.MaxLevel)
	{
		return 0;
	}

	return Rules.ExpBase + (Data.AccountLevel - 1) * Rules.ExpStep;
}

int32 UWarriorAccountSubsystem::GetInvestedPoints(FGameplayTag StatTag) const
{
	const int32* Invested = Data.InvestedStats.Find(StatTag);
	return Invested ? *Invested : 0;
}

float UWarriorAccountSubsystem::GetStatBonus(FGameplayTag StatTag) const
{
	const FStatDefinition* Definition = FindStatDefinition(StatTag);
	return Definition ? GetInvestedPoints(StatTag) * Definition->BonusPerPoint : 0.f;
}

int32 UWarriorAccountSubsystem::GetSkillCost(FGameplayTag SkillTag) const
{
	const int32* Cost = SkillCosts.Find(SkillTag);
	return Cost ? *Cost : -1;
}

TArray<FGameplayTag> UWarriorAccountSubsystem::GetInvestableStats() const
{
	TArray<FGameplayTag> Tags;
	for (const FStatDefinition& Definition : StatDefinitions)
	{
		Tags.Add(Definition.Tag);
	}
	return Tags;
}

TArray<FWarriorAccountStatBonus> UWarriorAccountSubsystem::BuildStatBonuses() const
{
	TArray<FWarriorAccountStatBonus> Bonuses;
	for (const FStatDefinition& Definition : StatDefinitions)
	{
		const int32 Invested = GetInvestedPoints(Definition.Tag);
		if (Invested <= 0)
		{
			continue;
		}

		FWarriorAccountStatBonus& Bonus = Bonuses.AddDefaulted_GetRef();
		Bonus.StatTag = Definition.Tag;
		Bonus.Attribute = Definition.Attribute;
		Bonus.Bonus = Invested * Definition.BonusPerPoint;
	}
	return Bonuses;
}

bool UWarriorAccountSubsystem::GetLastStageReward(FWarriorStageReward& OutReward) const
{
	if (!bHasLastReward)
	{
		return false;
	}

	OutReward = LastReward;
	return true;
}

bool UWarriorAccountSubsystem::InvestStatPoint(FGameplayTag StatTag, int32 Points)
{
	const FStatDefinition* Definition = FindStatDefinition(StatTag);
	if (!Definition)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] Invest ignored. %s is not an investable stat."), *StatTag.ToString());
		return false;
	}

	if (Points <= 0 || Data.StatPoints < Points)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Invest rejected. Need %d points, have %d."), Points, Data.StatPoints);
		return false;
	}

	const int32 Invested = GetInvestedPoints(StatTag);
	if (Invested + Points > Definition->MaxPoints)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Invest rejected. %s is limited to %d points (now %d)."), *StatTag.ToString(), Definition->MaxPoints, Invested);
		return false;
	}

	//TODO(server): 서버 연동 뒤에는 여기서 직접 바꾸지 않고 "투자 요청(스탯, 포인트, 요청 번호)"을 서버에 보낸다.
	//서버가 판정해 돌려준 계정 데이터를 ApplyServerSnapshot으로 적용하면 된다. 아래 변경 내역의 EntryId가 요청 번호다.
	Data.StatPoints -= Points;
	Data.InvestedStats.Add(StatTag, Invested + Points);
	AddLedgerEntry(EWarriorAccountLedgerType::StatInvest, StatPointName, -Points, StatTag);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Invested %d in %s. Now %d (bonus %.1f), %d points left."),
		Points, *StatTag.ToString(), Invested + Points, GetStatBonus(StatTag), Data.StatPoints);

	BroadcastAccountChanged();
	return true;
}

bool UWarriorAccountSubsystem::UnlockSkill(FGameplayTag SkillTag)
{
	const int32 Cost = GetSkillCost(SkillTag);
	if (Cost < 0)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] Unlock ignored. %s is not an unlockable skill."), *SkillTag.ToString());
		return false;
	}

	if (IsSkillUnlocked(SkillTag))
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Unlock ignored. %s is already unlocked."), *SkillTag.ToString());
		return false;
	}

	if (Data.StatPoints < Cost)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Unlock rejected. Need %d points, have %d."), Cost, Data.StatPoints);
		return false;
	}

	//TODO(server): InvestStatPoint와 같이, 서버 연동 뒤에는 서버에 요청하고 응답 스냅샷을 적용한다.
	Data.StatPoints -= Cost;
	Data.UnlockedSkills.AddTag(SkillTag);
	AddLedgerEntry(EWarriorAccountLedgerType::SkillUnlock, StatPointName, -Cost, SkillTag);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Unlocked %s for %d points. %d points left."), *SkillTag.ToString(), Cost, Data.StatPoints);

	BroadcastAccountChanged();
	return true;
}

void UWarriorAccountSubsystem::ApplyServerSnapshot(const FWarriorAccountData& InServerData)
{
	Data = InServerData;

	//서버가 확정한 값이므로, 그 시점까지의 로컬 변경 내역은 모두 서버가 반영한 것으로 본다.
	for (FWarriorAccountLedgerEntry& Entry : Ledger)
	{
		Entry.bServerConfirmed = true;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Server snapshot applied. Level %d, StatPoints %d"), Data.AccountLevel, Data.StatPoints);

	BroadcastAccountChanged();
}

void UWarriorAccountSubsystem::GetPendingLedgerEntries(TArray<FWarriorAccountLedgerEntry>& OutEntries) const
{
	OutEntries.Reset();
	for (const FWarriorAccountLedgerEntry& Entry : Ledger)
	{
		if (!Entry.bServerConfirmed)
		{
			OutEntries.Add(Entry);
		}
	}
}

void UWarriorAccountSubsystem::MarkLedgerConfirmed(const TArray<FGuid>& InEntryIds)
{
	for (FWarriorAccountLedgerEntry& Entry : Ledger)
	{
		if (InEntryIds.Contains(Entry.EntryId))
		{
			Entry.bServerConfirmed = true;
		}
	}
}

int64 UWarriorAccountSubsystem::GetExternalCurrency(FName InCurrency) const
{
	const int64* Balance = Data.ExternalCurrencies.Find(InCurrency);
	return Balance ? *Balance : 0;
}

void UWarriorAccountSubsystem::SetExternalCurrencyFromServer(FName InCurrency, int64 InBalance)
{
	const int64 OldBalance = GetExternalCurrency(InCurrency);
	if (OldBalance == InBalance)
	{
		return;
	}

	Data.ExternalCurrencies.Add(InCurrency, InBalance);

	//서버가 알려 준 변동이므로 이미 서버가 아는 값이다.
	AddLedgerEntry(EWarriorAccountLedgerType::ExternalCurrency, InCurrency, InBalance - OldBalance);
	Ledger.Last().bServerConfirmed = true;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Account] External currency %s: %lld -> %lld"), *InCurrency.ToString(), OldBalance, InBalance);

	BroadcastAccountChanged();
}

void UWarriorAccountSubsystem::HandleStageRecorded(const FWarriorStageRecord& InStageRecord)
{
	//중도 이탈은 보상이 없다.
	if (InStageRecord.Outcome != EWarriorStatOutcome::Cleared && InStageRecord.Outcome != EWarriorStatOutcome::Failed)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Account] No reward for record %s (not cleared or failed)."), *InStageRecord.RecordId.ToString());
		return;
	}

	//같은 기록으로 보상이 두 번 들어오지 않게 한다.
	if (Data.RewardedRecordIds.Contains(InStageRecord.RecordId))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] Record %s was already rewarded. Ignored."), *InStageRecord.RecordId.ToString());
		return;
	}

	const bool bCleared = InStageRecord.Outcome == EWarriorStatOutcome::Cleared;

	FWarriorStageReward Reward;
	Reward.RecordId = InStageRecord.RecordId;
	Reward.bCleared = bCleared;
	GrantExperience(bCleared ? Rules.ClearedExp : Rules.FailedExp, Reward);

	Data.RewardedRecordIds.Add(InStageRecord.RecordId);
	if (Data.RewardedRecordIds.Num() > Rules.MaxRewardedRecords)
	{
		Data.RewardedRecordIds.RemoveAt(0);
	}

	//TODO(server): 보상은 서버가 계산한다. 서버 연동 뒤에는 이 기록(또는 결과 요약)을 서버에 보내고,
	//서버가 돌려준 계정 데이터로 ApplyServerSnapshot을 호출한다. RecordId가 중복 지급 방지 키다.
	AddLedgerEntry(EWarriorAccountLedgerType::StageReward, ExpName, Reward.ExpGained, FGameplayTag(), InStageRecord.RecordId);
	if (Reward.StatPointsGained > 0)
	{
		AddLedgerEntry(EWarriorAccountLedgerType::StageReward, StatPointName, Reward.StatPointsGained, FGameplayTag(), InStageRecord.RecordId);
	}

	LastReward = Reward;
	bHasLastReward = true;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Account] Reward for %s: %s, Exp +%d, Level %d -> %d, StatPoints +%d (now %d)"),
		*InStageRecord.RecordId.ToString(),
		bCleared ? TEXT("Cleared") : TEXT("Failed"),
		Reward.ExpGained, Reward.LevelBefore, Reward.LevelAfter, Reward.StatPointsGained, Data.StatPoints);

	OnStageRewarded.Broadcast(Reward);
	BroadcastAccountChanged();
}

void UWarriorAccountSubsystem::BuildDefinitions()
{
	//임시 수치. 기획 확정 시 조정한다. 어트리뷰트는 UWarriorAttributeSet의 것과 1:1로 연결된다.
	StatDefinitions.Reset();

	auto AddStat = [this](const FGameplayTag& InTag, const FGameplayAttribute& InAttribute, float InBonusPerPoint, int32 InMaxPoints)
	{
		FStatDefinition& Definition = StatDefinitions.AddDefaulted_GetRef();
		Definition.Tag = InTag;
		Definition.Attribute = InAttribute;
		Definition.BonusPerPoint = InBonusPerPoint;
		Definition.MaxPoints = InMaxPoints;
	};

	AddStat(WarriorAccountTags::Account_Stat_MaxHealth.GetTag(), UWarriorAttributeSet::GetMaxHealthAttribute(), 10.f, 10);
	AddStat(WarriorAccountTags::Account_Stat_MaxStamina.GetTag(), UWarriorAttributeSet::GetMaxStaminaAttribute(), 5.f, 10);
	AddStat(WarriorAccountTags::Account_Stat_AttackPower.GetTag(), UWarriorAttributeSet::GetAttackPowerAttribute(), 2.f, 10);
	AddStat(WarriorAccountTags::Account_Stat_DefensePower.GetTag(), UWarriorAttributeSet::GetDefensePowerAttribute(), 1.f, 10);

	SkillCosts.Reset();
	SkillCosts.Add(WarriorAccountTags::Account_Skill_Combo4.GetTag(), 2);
}

const UWarriorAccountSubsystem::FStatDefinition* UWarriorAccountSubsystem::FindStatDefinition(const FGameplayTag& InStatTag) const
{
	return StatDefinitions.FindByPredicate([&InStatTag](const FStatDefinition& Definition)
	{
		return Definition.Tag == InStatTag;
	});
}

void UWarriorAccountSubsystem::GrantExperience(int32 InExp, FWarriorStageReward& OutReward)
{
	OutReward.ExpGained = FMath::Max(0, InExp);
	OutReward.LevelBefore = Data.AccountLevel;

	Data.Experience += OutReward.ExpGained;

	while (Data.AccountLevel < Rules.MaxLevel && Data.Experience >= GetExperienceToNextLevel())
	{
		Data.Experience -= GetExperienceToNextLevel();
		++Data.AccountLevel;
		Data.StatPoints += Rules.StatPointsPerLevel;
		OutReward.StatPointsGained += Rules.StatPointsPerLevel;
	}

	//최대 레벨에서는 남는 경험치를 쌓지 않는다.
	if (Data.AccountLevel >= Rules.MaxLevel)
	{
		Data.Experience = 0;
	}

	OutReward.LevelAfter = Data.AccountLevel;
}

void UWarriorAccountSubsystem::AddLedgerEntry(EWarriorAccountLedgerType InType, FName InCurrency, int64 InDelta, const FGameplayTag& InTarget, const FGuid& InSourceId)
{
	FWarriorAccountLedgerEntry& Entry = Ledger.AddDefaulted_GetRef();
	Entry.EntryId = FGuid::NewGuid();
	Entry.Type = InType;
	Entry.Currency = InCurrency;
	Entry.Delta = InDelta;
	Entry.Target = InTarget;
	Entry.SourceId = InSourceId;
	Entry.TimeUtc = FDateTime::UtcNow();

	if (Ledger.Num() > Rules.MaxLedgerEntries)
	{
		Ledger.RemoveAt(0);
	}
}

void UWarriorAccountSubsystem::BroadcastAccountChanged()
{
	OnAccountChanged.Broadcast(Data);
}
