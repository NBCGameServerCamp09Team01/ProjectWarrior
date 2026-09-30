#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "WarriorProfileStatsSubsystem.h"
#include "WarriorStageStatsSubsystem.h"
#include "WarriorStatTypes.h"

/**
 * 통계 확인용 콘솔 명령 (S1-7). PIE 중 ~ 콘솔에서 입력하면 Output Log에 [Stats]로 출력된다.
 *   Stats.Dump       누적 통계 + 진행 중인 판 요약
 *   Stats.DumpStage  진행 중인 스테이지 (없으면 마지막 스테이지 기록)
 *   Stats.DumpRun    진행 중인 판 (없으면 마지막 판 기록)
 *   Stats.Reset      GameInstance에 보관된 기록 전부 삭제
 */
namespace WarriorStatsConsole
{
	void Line(const FString& Text)
	{
		UE_LOG(LogProjectWarrior, Display, TEXT("[Stats] %s"), *Text);
	}

	FString OutcomeName(const EWarriorStatOutcome Outcome)
	{
		return StaticEnum<EWarriorStatOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome));
	}

	FString ValueToString(const int32 Value) { return FString::FromInt(Value); }
	FString ValueToString(const double Value) { return FString::Printf(TEXT("%.1f"), Value); }

	template <typename ValueType>
	FString MapToString(const TMap<FName, ValueType>& Map)
	{
		if (Map.IsEmpty())
		{
			return TEXT("-");
		}
		TArray<FString> Parts;
		for (const TPair<FName, ValueType>& Pair : Map)
		{
			Parts.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key.ToString(), *ValueToString(Pair.Value)));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	void DumpStatBlock(const FWarriorStatBlock& Stats)
	{
		const FWarriorAttackStats& A = Stats.Attack;
		Line(FString::Printf(TEXT("  Play time      %.1f s"), Stats.PlayTimeSeconds));
		Line(FString::Printf(TEXT("  Attack         attempts %d, landed %d (hit rate %.0f%%), hits %d"),
			A.AttackAttempts, A.AttacksLanded, A.GetHitRate() * 100.0, A.HitsDealt));
		Line(FString::Printf(TEXT("  Damage dealt   total %.1f, avg %.1f, max %.1f"), A.DamageDealt, A.GetAverageDamage(), A.MaxDamageDealt));
		Line(FString::Printf(TEXT("  Kills          %d (max per attack %d)"), A.Kills, A.MaxKillsPerAttack));
		Line(FString::Printf(TEXT("    by enemy     %s"), *MapToString(A.KillsByEnemyType)));
		Line(FString::Printf(TEXT("    by death     %s"), *MapToString(A.KillsByDeathType)));
		Line(FString::Printf(TEXT("    dmg/ability  %s"), *MapToString(A.DamageByAbility)));

		const FWarriorDefenseStats& D = Stats.Defense;
		Line(FString::Printf(TEXT("  Defense        hits taken %d, damage taken %.1f, deaths %d"), D.HitsTaken, D.DamageTaken, D.Deaths));
		Line(FString::Printf(TEXT("    by enemy     %s"), *MapToString(D.DamageTakenByEnemyType)));

		const FWarriorHealStats& H = Stats.Heal;
		Line(FString::Printf(TEXT("  Heal           amount %.1f, overheal %.1f, potions %d (%s)"),
			H.HealAmount, H.Overheal, H.PotionsUsed, *MapToString(H.PotionsUsedByItem)));

		const FWarriorEconomyStats& E = Stats.Economy;
		Line(FString::Printf(TEXT("  Gold           earned %d (%s), spent %d"), E.GoldEarned, *MapToString(E.GoldEarnedBySource), E.GoldSpent));
		if (E.PurchasesByItem.IsEmpty())
		{
			Line(TEXT("  Purchases      -"));
		}
		for (const TPair<FName, FWarriorItemPurchaseStats>& Pair : E.PurchasesByItem)
		{
			Line(FString::Printf(TEXT("  Purchase       %s x%d, %d gold"), *Pair.Key.ToString(), Pair.Value.Count, Pair.Value.GoldSpent));
		}

		for (const FWarriorStatEntry& Entry : Stats.Extra)
		{
			Line(FString::Printf(TEXT("  Extra          %s%s: sum %.2f, max %.2f, min %.2f, count %d"),
				*Entry.StatTag.ToString(),
				Entry.DimensionKey.IsNone() ? TEXT("") : *FString::Printf(TEXT(" [%s]"), *Entry.DimensionKey.ToString()),
				Entry.Value.Sum, Entry.Value.Max, Entry.Value.Min, Entry.Value.Count));
		}
	}

	void DumpStageRecord(const FWarriorStageRecord& Record)
	{
		Line(FString::Printf(TEXT("===== Stage %s (%s) ====="), *Record.StageId.ToString(), *OutcomeName(Record.Outcome)));
		Line(FString::Printf(TEXT("  Record %s / Run %s / Build '%s'"), *Record.RecordId.ToString(), *Record.RunId.ToString(), *Record.BuildVersion));
		Line(FString::Printf(TEXT("  Waves cleared  %d / %d, gold at end %d"), Record.WavesCleared, Record.TotalWaves, Record.GoldAtEnd));
		DumpStatBlock(Record.Stats);

		for (const FWarriorWaveRecord& Wave : Record.Waves)
		{
			Line(FString::Printf(TEXT("  Wave %d%s      start %.1f s, %s, kills %d"),
				Wave.WaveNumber, Wave.bBossWave ? TEXT(" (boss)") : TEXT(""), Wave.StartTimeSeconds,
				Wave.bCleared ? *FString::Printf(TEXT("cleared at %.1f s (%.1f s)"), Wave.ClearTimeSeconds, Wave.ClearTimeSeconds - Wave.StartTimeSeconds) : TEXT("not cleared"),
				Wave.Kills));
		}
		for (const FWarriorEnemyTypeStats& Enemy : Record.Enemies)
		{
			Line(FString::Printf(TEXT("  Enemy %s  spawned %d, killed %d, avg time to kill %.1f s, damage to player %.1f, player kills %d"),
				*Enemy.EnemyType.ToString(), Enemy.Spawned, Enemy.Killed, Enemy.GetAverageTimeToKill(), Enemy.DamageToPlayer, Enemy.PlayerKills));
		}
		for (const FWarriorDeathRecord& Death : Record.Deaths)
		{
			Line(FString::Printf(TEXT("  Death          wave %d at %.1f s by %s (%s)"),
				Death.WaveNumber, Death.TimeSeconds, *Death.KillerEnemyType.ToString(), *Death.KillerAbility.ToString()));
		}
		for (const FWarriorPurchaseRecord& Purchase : Record.Purchases)
		{
			Line(FString::Printf(TEXT("  Bought         %s x%d, %d gold, wave %d at %.1f s"),
				*Purchase.ItemId.ToString(), Purchase.Count, Purchase.GoldSpent, Purchase.WaveNumber, Purchase.TimeSeconds));
		}
	}

	void DumpRunRecord(const FWarriorRunRecord& Run)
	{
		Line(FString::Printf(TEXT("===== Run %s (%s) ====="), *Run.RunId.ToString(), *OutcomeName(Run.Outcome)));
		Line(FString::Printf(TEXT("  Stages         played %d, cleared %d, build '%s'"), Run.StagesPlayed, Run.StagesCleared, *Run.BuildVersion));
		DumpStatBlock(Run.Stats);
	}

	UWarriorProfileStatsSubsystem* GetProfile(UWorld* World)
	{
		UWarriorProfileStatsSubsystem* Profile = UWarriorProfileStatsSubsystem::Get(World);
		if (!Profile)
		{
			Line(TEXT("No profile stats subsystem (run this during PIE or in game)."));
		}
		return Profile;
	}

	void Dump(UWorld* World)
	{
		UWarriorProfileStatsSubsystem* Profile = GetProfile(World);
		if (!Profile)
		{
			return;
		}

		const FWarriorLifetimeStats& Lifetime = Profile->GetLifetimeStats();
		Line(TEXT("===== Lifetime (this session) ====="));
		Line(FString::Printf(TEXT("  Runs           played %d, cleared %d"), Lifetime.RunsPlayed, Lifetime.RunsCleared));
		Line(FString::Printf(TEXT("  Stages         played %d, cleared %d, failed %d"), Lifetime.StagesPlayed, Lifetime.StagesCleared, Lifetime.StagesFailed));
		Line(FString::Printf(TEXT("  Best clear     %.1f s, max wave %d"), Lifetime.BestStageClearTimeSeconds, Lifetime.MaxWaveReached));
		DumpStatBlock(Lifetime.Stats);
		for (const TPair<FName, FWarriorEnemyTypeStats>& Pair : Lifetime.EnemiesByType)
		{
			const FWarriorEnemyTypeStats& Enemy = Pair.Value;
			Line(FString::Printf(TEXT("  Enemy %s  spawned %d, killed %d, avg time to kill %.1f s, player kills %d"),
				*Pair.Key.ToString(), Enemy.Spawned, Enemy.Killed, Enemy.GetAverageTimeToKill(), Enemy.PlayerKills));
		}
		Line(FString::Printf(TEXT("  Stored records stages %d / runs %d, active run: %s"),
			Profile->GetStageRecords().Num(), Profile->GetRunRecords().Num(), Profile->HasActiveRun() ? TEXT("yes") : TEXT("no")));
	}

	void DumpStage(UWorld* World)
	{
		if (const UWarriorStageStatsSubsystem* StageStats = UWarriorStageStatsSubsystem::Get(World); StageStats && StageStats->IsRecording())
		{
			Line(FString::Printf(TEXT("(in progress, stage time %.1f s, live play time / current gold shown)"), StageStats->GetStageTimeSeconds()));
			FWarriorStageRecord LiveRecord = StageStats->GetCurrentStageRecord();
			LiveRecord.Stats.PlayTimeSeconds = StageStats->GetLivePlayTimeSeconds();
			LiveRecord.GoldAtEnd = StageStats->GetCurrentGold();
			DumpStageRecord(LiveRecord);
			return;
		}

		UWarriorProfileStatsSubsystem* Profile = GetProfile(World);
		FWarriorStageRecord LastRecord;
		if (Profile && Profile->GetLastStageRecord(LastRecord))
		{
			Line(TEXT("(no stage in progress, showing the last stage record)"));
			DumpStageRecord(LastRecord);
		}
		else if (Profile)
		{
			Line(TEXT("No stage record yet."));
		}
	}

	void DumpRun(UWorld* World)
	{
		UWarriorProfileStatsSubsystem* Profile = GetProfile(World);
		if (!Profile)
		{
			return;
		}
		if (Profile->HasActiveRun())
		{
			Line(TEXT("(in progress: totals include finished stages only)"));
			DumpRunRecord(Profile->GetCurrentRun());
			return;
		}

		FWarriorRunRecord LastRun;
		if (Profile->GetLastRunRecord(LastRun))
		{
			Line(TEXT("(no run in progress, showing the last run record)"));
			DumpRunRecord(LastRun);
		}
		else
		{
			Line(TEXT("No run record yet."));
		}
	}

	void Reset(UWorld* World)
	{
		if (UWarriorProfileStatsSubsystem* Profile = GetProfile(World))
		{
			Profile->ResetAll();
			Line(TEXT("All stored stats records were cleared. (The stage in progress keeps recording.)"));
		}
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Stats.Dump"), TEXT("누적 통계와 진행 중인 판 요약을 로그로 출력"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorld DumpStageCommand(
		TEXT("Stats.DumpStage"), TEXT("진행 중인 스테이지(없으면 마지막 스테이지 기록)를 로그로 출력"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&DumpStage));

	FAutoConsoleCommandWithWorld DumpRunCommand(
		TEXT("Stats.DumpRun"), TEXT("진행 중인 판(없으면 마지막 판 기록)을 로그로 출력"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&DumpRun));

	FAutoConsoleCommandWithWorld ResetCommand(
		TEXT("Stats.Reset"), TEXT("GameInstance에 보관된 통계 기록을 모두 삭제 (디버그용)"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Reset));
}

#endif // !UE_BUILD_SHIPPING
