#include "WarriorStatTypes.h"

namespace WarriorStatTypes_Private
{
	template <typename ValueType>
	void AddMap(TMap<FName, ValueType>& Target, const TMap<FName, ValueType>& Source)
	{
		for (const TPair<FName, ValueType>& Pair : Source)
		{
			Target.FindOrAdd(Pair.Key) += Pair.Value;
		}
	}
}

void FWarriorStatValue::Add(const double InValue)
{
	Max = Count == 0 ? InValue : FMath::Max(Max, InValue);
	Min = Count == 0 ? InValue : FMath::Min(Min, InValue);
	Sum += InValue;
	++Count;
}

void FWarriorStatValue::Merge(const FWarriorStatValue& Other)
{
	if (Other.Count == 0)
	{
		return;
	}

	Max = Count == 0 ? Other.Max : FMath::Max(Max, Other.Max);
	Min = Count == 0 ? Other.Min : FMath::Min(Min, Other.Min);
	Sum += Other.Sum;
	Count += Other.Count;
}

double FWarriorAttackStats::GetHitRate() const
{
	return AttackAttempts > 0 ? static_cast<double>(AttacksLanded) / AttackAttempts : 0.0;
}

double FWarriorAttackStats::GetAverageDamage() const
{
	return HitsDealt > 0 ? DamageDealt / HitsDealt : 0.0;
}

void FWarriorAttackStats::Merge(const FWarriorAttackStats& Other)
{
	AttackAttempts += Other.AttackAttempts;
	AttacksLanded += Other.AttacksLanded;
	HitsDealt += Other.HitsDealt;
	DamageDealt += Other.DamageDealt;
	MaxDamageDealt = FMath::Max(MaxDamageDealt, Other.MaxDamageDealt);
	Kills += Other.Kills;
	MaxKillsPerAttack = FMath::Max(MaxKillsPerAttack, Other.MaxKillsPerAttack);
	WarriorStatTypes_Private::AddMap(KillsByEnemyType, Other.KillsByEnemyType);
	WarriorStatTypes_Private::AddMap(KillsByDeathType, Other.KillsByDeathType);
	WarriorStatTypes_Private::AddMap(DamageByAbility, Other.DamageByAbility);
}

void FWarriorDefenseStats::Merge(const FWarriorDefenseStats& Other)
{
	HitsTaken += Other.HitsTaken;
	DamageTaken += Other.DamageTaken;
	Deaths += Other.Deaths;
	WarriorStatTypes_Private::AddMap(DamageTakenByEnemyType, Other.DamageTakenByEnemyType);
}

void FWarriorHealStats::Merge(const FWarriorHealStats& Other)
{
	HealAmount += Other.HealAmount;
	Overheal += Other.Overheal;
	PotionsUsed += Other.PotionsUsed;
	WarriorStatTypes_Private::AddMap(PotionsUsedByItem, Other.PotionsUsedByItem);
}

void FWarriorEconomyStats::Merge(const FWarriorEconomyStats& Other)
{
	GoldEarned += Other.GoldEarned;
	GoldSpent += Other.GoldSpent;
	WarriorStatTypes_Private::AddMap(GoldEarnedBySource, Other.GoldEarnedBySource);
	for (const TPair<FName, FWarriorItemPurchaseStats>& Pair : Other.PurchasesByItem)
	{
		FWarriorItemPurchaseStats& Purchase = PurchasesByItem.FindOrAdd(Pair.Key);
		Purchase.Count += Pair.Value.Count;
		Purchase.GoldSpent += Pair.Value.GoldSpent;
	}
}

void FWarriorStatBlock::AddExtra(const FGameplayTag& InStatTag, const double InValue, const FName InDimensionKey)
{
	if (!InStatTag.IsValid())
	{
		return;
	}

	FindOrAddExtra(InStatTag, InDimensionKey).Add(InValue);
}

const FWarriorStatValue* FWarriorStatBlock::FindExtra(const FGameplayTag& InStatTag, const FName InDimensionKey) const
{
	const FWarriorStatEntry* Entry = Extra.FindByPredicate([&](const FWarriorStatEntry& Candidate)
	{
		return Candidate.StatTag == InStatTag && Candidate.DimensionKey == InDimensionKey;
	});
	return Entry ? &Entry->Value : nullptr;
}

FWarriorStatValue& FWarriorStatBlock::FindOrAddExtra(const FGameplayTag& InStatTag, const FName InDimensionKey)
{
	FWarriorStatEntry* Entry = Extra.FindByPredicate([&](const FWarriorStatEntry& Candidate)
	{
		return Candidate.StatTag == InStatTag && Candidate.DimensionKey == InDimensionKey;
	});
	if (!Entry)
	{
		Entry = &Extra.AddDefaulted_GetRef();
		Entry->StatTag = InStatTag;
		Entry->DimensionKey = InDimensionKey;
	}
	return Entry->Value;
}

void FWarriorStatBlock::Merge(const FWarriorStatBlock& Other)
{
	Attack.Merge(Other.Attack);
	Defense.Merge(Other.Defense);
	Heal.Merge(Other.Heal);
	Economy.Merge(Other.Economy);
	PlayTimeSeconds += Other.PlayTimeSeconds;
	for (const FWarriorStatEntry& Entry : Other.Extra)
	{
		FindOrAddExtra(Entry.StatTag, Entry.DimensionKey).Merge(Entry.Value);
	}
}

double FWarriorEnemyTypeStats::GetAverageTimeToKill() const
{
	return Killed > 0 ? TotalTimeToKill / Killed : 0.0;
}

void FWarriorEnemyTypeStats::Merge(const FWarriorEnemyTypeStats& Other)
{
	if (EnemyType.IsNone())
	{
		EnemyType = Other.EnemyType;
	}
	Spawned += Other.Spawned;
	Killed += Other.Killed;
	TotalTimeToKill += Other.TotalTimeToKill;
	DamageToPlayer += Other.DamageToPlayer;
	PlayerKills += Other.PlayerKills;
}

FWarriorEnemyTypeStats& FWarriorStageRecord::FindOrAddEnemyType(const FName InEnemyType)
{
	FWarriorEnemyTypeStats* Found = Enemies.FindByPredicate([InEnemyType](const FWarriorEnemyTypeStats& Candidate)
	{
		return Candidate.EnemyType == InEnemyType;
	});
	if (!Found)
	{
		Found = &Enemies.AddDefaulted_GetRef();
		Found->EnemyType = InEnemyType;
	}
	return *Found;
}
