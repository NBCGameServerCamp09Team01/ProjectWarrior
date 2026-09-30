#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Stats/WarriorProfileStatsSubsystem.h"
#include "ProjectWarrior/Stats/WarriorStageStatsSubsystem.h"
#include "ProjectWarrior/Stats/WarriorStatTags.h"
#include "ProjectWarrior/Stats/WarriorStatsLibrary.h"

/**
 * S1 통계 자동화 테스트.
 * 테스트 월드는 BeginPlay 전이라 GameMode·GameState가 없고 다이나믹 델리게이트 방송도 막힌다.
 * 그래서 스테이지 기록은 FWarriorStatsStageTest로 기록 상태를 직접 열고, 핸들러를 직접 호출해 검증한다.
 */
class FWarriorStatsStageTest
{
public:
	/** GameState 없이 스테이지 기록을 연다 */
	static void BeginFakeStage(UWarriorStageStatsSubsystem& Stats)
	{
		Stats.Record = FWarriorStageRecord();
		Stats.Record.RecordId = FGuid::NewGuid();
		Stats.Record.StageId = TEXT("L_Test_Stats");
		Stats.ResetTracking();
		Stats.CurrentState = EWarriorStageState::InProgress;
		Stats.CurrentWaveIndex = INDEX_NONE;
		Stats.bPlayerHitThisWave = false;
		Stats.bRecording = true;
		Stats.bRecordSubmitted = false;
	}

	/** 월드 정리 전에 기록을 닫아 Deinitialize가 Abandoned로 넘기지 않게 한다 */
	static void EndFakeStage(UWarriorStageStatsSubsystem& Stats)
	{
		Stats.bRecording = false;
	}

	/** 공격 시도 기록(RecordAttackAttempt) 없이 공격 번호를 바꾼다. 어빌리티 없는 컨텍스트는 null 키를 쓴다 */
	static void SetAttackSerial(UWarriorStageStatsSubsystem& Stats, const uint64 Serial)
	{
		Stats.AttackSerials.FindOrAdd(FObjectKey()) = Serial;
	}

	static void ChangeState(UWarriorStageStatsSubsystem& Stats, const EWarriorStageState NewState)
	{
		Stats.HandleStageStateChanged(NewState, Stats.CurrentState);
	}

	static void ChangeWave(UWarriorStageStatsSubsystem& Stats, const int32 WaveNumber, const int32 TotalWaves, const bool bBossWave)
	{
		Stats.HandleWaveChanged(WaveNumber, TotalWaves, bBossWave);
	}

	static bool IsSubmitted(const UWarriorStageStatsSubsystem& Stats) { return Stats.bRecordSubmitted; }
};

namespace WarriorStatsTests_Private
{
	const FName TestEnemyType(TEXT("WarriorAICharacter"));

	FWarriorItemPurchaseStats MakePurchase(const int32 Count, const int32 GoldSpent)
	{
		FWarriorItemPurchaseStats Purchase;
		Purchase.Count = Count;
		Purchase.GoldSpent = GoldSpent;
		return Purchase;
	}

	/** 테스트 월드와 스테이지 기록기. 소멸 시 기록을 닫고 월드를 정리한다 */
	struct FStageTestWorld
	{
		UWorld* World = nullptr;
		UWarriorStageStatsSubsystem* Stats = nullptr;

		FStageTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				Stats = World->GetSubsystem<UWarriorStageStatsSubsystem>();
				if (!Stats)
				{
					Stats = NewObject<UWarriorStageStatsSubsystem>(World);
				}
			}
		}

		~FStageTestWorld()
		{
			if (Stats)
			{
				FWarriorStatsStageTest::EndFakeStage(*Stats);
			}
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		template <typename ActorType>
		ActorType* Spawn(const FVector& Location = FVector::ZeroVector)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ActorType>(ActorType::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}

		/** PlayerState가 붙은 폰. IsPlayerControlled()가 true가 된다 */
		APawn* SpawnPlayer()
		{
			APawn* Player = Spawn<APawn>();
			APlayerState* PlayerState = Spawn<APlayerState>();
			if (Player && PlayerState)
			{
				Player->SetPlayerState(PlayerState);
			}
			return Player;
		}
	};

	FGameplayEffectContextHandle MakeContext(AActor* Instigator)
	{
		FGameplayEffectContextHandle Context(new FGameplayEffectContext());
		Context.AddInstigator(Instigator, Instigator);
		return Context;
	}

	FWarriorStageRecord MakeStageRecord(const EWarriorStatOutcome Outcome, const int32 Kills, const int32 Gold, const double PlayTime, const int32 LastWave)
	{
		FWarriorStageRecord Record;
		Record.RecordId = FGuid::NewGuid();
		Record.StageId = TEXT("L_Test_Stats");
		Record.Outcome = Outcome;
		Record.Stats.Attack.Kills = Kills;
		Record.Stats.Attack.KillsByEnemyType.Add(TestEnemyType, Kills);
		Record.Stats.Economy.GoldEarned = Gold;
		Record.Stats.PlayTimeSeconds = PlayTime;
		for (int32 Wave = 1; Wave <= LastWave; ++Wave)
		{
			FWarriorWaveRecord& WaveRecord = Record.Waves.AddDefaulted_GetRef();
			WaveRecord.WaveNumber = Wave;
			WaveRecord.bCleared = true;
		}
		Record.WavesCleared = LastWave;
		FWarriorEnemyTypeStats& Enemy = Record.FindOrAddEnemyType(TestEnemyType);
		Enemy.Spawned = Kills;
		Enemy.Killed = Kills;
		return Record;
	}

	UWarriorProfileStatsSubsystem* MakeProfile()
	{
		// GameInstance 서브시스템은 Outer가 GameInstance여야 한다 (ClassWithin). 서브시스템 초기화는 거치지 않는다.
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		return GameInstance ? NewObject<UWarriorProfileStatsSubsystem>(GameInstance) : nullptr;
	}
}

namespace WST = WarriorStatsTests_Private;

#define WARRIOR_STATS_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

//~ 값 합치기

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsStatBlockMergeTest, "ProjectWarrior.Stats.S1.StatBlockMerge", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsStatBlockMergeTest::RunTest(const FString& Parameters)
{
	// 첫 값은 음수여도 최대·최소가 된다.
	FWarriorStatValue Value;
	Value.Add(-2.0);
	TestEqual(TEXT("First value sets max"), Value.Max, -2.0);
	TestEqual(TEXT("First value sets min"), Value.Min, -2.0);
	Value.Add(5.0);
	TestEqual(TEXT("Sum"), Value.Sum, 3.0);
	TestEqual(TEXT("Max"), Value.Max, 5.0);
	TestEqual(TEXT("Min"), Value.Min, -2.0);
	TestEqual(TEXT("Count"), Value.Count, 2);

	// 빈 값을 합쳐도 최소가 0으로 바뀌지 않는다.
	Value.Merge(FWarriorStatValue());
	TestEqual(TEXT("Empty merge keeps min"), Value.Min, -2.0);
	TestEqual(TEXT("Empty merge keeps count"), Value.Count, 2);

	FWarriorStatValue Empty;
	FWarriorStatValue Other;
	Other.Add(0.5);
	Empty.Merge(Other);
	TestEqual(TEXT("Merge into empty takes other min"), Empty.Min, 0.5);

	const FName Kill(TEXT("Kill"));
	const FName Potion(TEXT("Potion_S"));
	const FGameplayTag Overkill = WarriorStatTags::Stat_Combat_Damage_Overkill;

	FWarriorStatBlock A;
	A.Attack.Kills = 2;
	A.Attack.MaxDamageDealt = 40.0;
	A.Attack.MaxKillsPerAttack = 2;
	A.Attack.KillsByEnemyType.Add(WST::TestEnemyType, 2);
	A.Economy.GoldEarned = 10;
	A.Economy.GoldEarnedBySource.Add(Kill, 10);
	A.Economy.PurchasesByItem.Add(Potion, WST::MakePurchase(1, 10));
	A.PlayTimeSeconds = 30.0;
	A.AddExtra(Overkill, 5.0, WST::TestEnemyType);

	FWarriorStatBlock B;
	B.Attack.Kills = 3;
	B.Attack.MaxDamageDealt = 25.0;
	B.Attack.MaxKillsPerAttack = 1;
	B.Attack.KillsByEnemyType.Add(WST::TestEnemyType, 3);
	B.Economy.GoldEarned = 7;
	B.Economy.GoldEarnedBySource.Add(Kill, 7);
	B.Economy.PurchasesByItem.Add(Potion, WST::MakePurchase(2, 20));
	B.PlayTimeSeconds = 12.5;
	B.AddExtra(Overkill, 8.0, WST::TestEnemyType);
	B.AddExtra(WarriorStatTags::Stat_Defense_Wave_NoHit, 1.0);

	A.Merge(B);
	TestEqual(TEXT("Kills add"), A.Attack.Kills, 5);
	TestEqual(TEXT("Max damage keeps larger"), A.Attack.MaxDamageDealt, 40.0);
	TestEqual(TEXT("Max kills per attack keeps larger"), A.Attack.MaxKillsPerAttack, 2);
	TestEqual(TEXT("Kills by enemy add"), A.Attack.KillsByEnemyType.FindRef(WST::TestEnemyType), 5);
	TestEqual(TEXT("Gold adds"), A.Economy.GoldEarned, 17);
	TestEqual(TEXT("Gold by source adds"), A.Economy.GoldEarnedBySource.FindRef(Kill), 17);
	TestEqual(TEXT("Purchase count adds"), A.Economy.PurchasesByItem.FindRef(Potion).Count, 3);
	TestEqual(TEXT("Purchase gold adds"), A.Economy.PurchasesByItem.FindRef(Potion).GoldSpent, 30);
	TestEqual(TEXT("Play time adds"), A.PlayTimeSeconds, 42.5);

	const FWarriorStatValue* OverkillValue = A.FindExtra(Overkill, WST::TestEnemyType);
	if (TestNotNull(TEXT("Overkill extra"), OverkillValue))
	{
		TestEqual(TEXT("Overkill sum"), OverkillValue->Sum, 13.0);
		TestEqual(TEXT("Overkill max"), OverkillValue->Max, 8.0);
		TestEqual(TEXT("Overkill count"), OverkillValue->Count, 2);
	}
	TestNotNull(TEXT("Extra only in B is added"), A.FindExtra(WarriorStatTags::Stat_Defense_Wave_NoHit));
	TestNull(TEXT("Dimension key is part of the extra key"), A.FindExtra(Overkill));
	return true;
}

//~ 파생 값

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsDerivedValuesTest, "ProjectWarrior.Stats.S1.DerivedValues", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsDerivedValuesTest::RunTest(const FString& Parameters)
{
	FWarriorAttackStats Attack;
	TestEqual(TEXT("Hit rate without attempts"), Attack.GetHitRate(), 0.0);
	TestEqual(TEXT("Average damage without hits"), Attack.GetAverageDamage(), 0.0);

	Attack.AttackAttempts = 4;
	Attack.AttacksLanded = 3;
	Attack.HitsDealt = 5;
	Attack.DamageDealt = 100.0;
	TestEqual(TEXT("Hit rate = landed / attempts"), Attack.GetHitRate(), 0.75);
	TestEqual(TEXT("Average damage = damage / hits"), Attack.GetAverageDamage(), 20.0);
	TestEqual(TEXT("Library hit rate"), UWarriorStatsLibrary::GetHitRate(Attack), 0.75);

	FWarriorEnemyTypeStats Enemy;
	TestEqual(TEXT("Average time to kill without kills"), Enemy.GetAverageTimeToKill(), 0.0);
	Enemy.Killed = 4;
	Enemy.TotalTimeToKill = 30.0;
	TestEqual(TEXT("Average time to kill"), Enemy.GetAverageTimeToKill(), 7.5);

	// 적 종류 합치기: 빈 종류는 상대 종류를 받는다.
	FWarriorEnemyTypeStats Merged;
	Merged.Merge(Enemy);
	TestEqual(TEXT("Enemy merge adds kills"), Merged.Killed, 4);

	FWarriorStatBlock Stats;
	FWarriorStatValue Found;
	TestFalse(TEXT("Missing extra"), UWarriorStatsLibrary::FindExtraStat(Stats, WarriorStatTags::Stat_Time_Rest, NAME_None, Found));
	Stats.AddExtra(WarriorStatTags::Stat_Time_Rest, 3.0);
	Stats.AddExtra(FGameplayTag(), 99.0);
	TestTrue(TEXT("Found extra"), UWarriorStatsLibrary::FindExtraStat(Stats, WarriorStatTags::Stat_Time_Rest, NAME_None, Found));
	TestEqual(TEXT("Found extra sum"), Found.Sum, 3.0);
	TestEqual(TEXT("Invalid tag is ignored"), Stats.Extra.Num(), 1);
	return true;
}

//~ 데미지와 처치

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsDamageAndKillTest, "ProjectWarrior.Stats.S1.DamageAndKill", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsDamageAndKillTest::RunTest(const FString& Parameters)
{
	WST::FStageTestWorld Test;
	if (!TestNotNull(TEXT("World"), Test.World) || !TestNotNull(TEXT("Stage stats"), Test.Stats))
	{
		return false;
	}
	UWarriorStageStatsSubsystem& Stats = *Test.Stats;

	APawn* Player = Test.SpawnPlayer();
	TArray<AWarriorAICharacter*> Enemies;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Enemies.Add(Test.Spawn<AWarriorAICharacter>(FVector(Index * 500.0, 0.0, 200.0)));
	}
	if (!TestNotNull(TEXT("Player"), Player) || Enemies.Contains(nullptr))
	{
		AddError(TEXT("Failed to spawn test actors."));
		return false;
	}
	if (!TestTrue(TEXT("Test player counts as player"), UWarriorStageStatsSubsystem::IsPlayerActor(Player)))
	{
		return false;
	}
	TestFalse(TEXT("Enemy does not count as player"), UWarriorStageStatsSubsystem::IsPlayerActor(Enemies[0]));

	FWarriorStatsStageTest::BeginFakeStage(Stats);
	FWarriorStatsStageTest::ChangeWave(Stats, 1, 1, false);
	for (AWarriorAICharacter* Enemy : Enemies)
	{
		Stats.HandleEnemySpawned(Enemy);
	}

	const FGameplayEffectContextHandle PlayerHit = WST::MakeContext(Player);

	// 공격 1: 적 0, 1을 한 번에 처치 (한 번에 2킬)
	FWarriorStatsStageTest::SetAttackSerial(Stats, 1);
	Stats.HandleDamage(PlayerHit, Enemies[0], 30.0f, 0.0f, true);
	Stats.HandleDamage(PlayerHit, Enemies[1], 50.0f, 10.0f, true);
	Stats.HandleEnemyKilled(Enemies[0], TEXT("Normal"));
	Stats.HandleEnemyKilled(Enemies[1], TEXT("Normal"));

	// 공격 2: 적 2 처치
	FWarriorStatsStageTest::SetAttackSerial(Stats, 2);
	Stats.HandleDamage(PlayerHit, Enemies[2], 20.0f, 0.0f, true);
	Stats.HandleEnemyKilled(Enemies[2], TEXT("Finisher"));

	// 적 3은 다른 적에게 막타를 맞았다: 플레이어 처치가 아니다.
	Stats.HandleDamage(WST::MakeContext(Enemies[4]), Enemies[3], 15.0f, 0.0f, true);
	Stats.HandleEnemyKilled(Enemies[3], TEXT("Normal"));

	// 적 4는 데미지 기록 없이 죽었다: 플레이어 처치로 세고 사망 방식을 모르면 Unknown.
	Stats.HandleEnemyKilled(Enemies[4], NAME_None);

	const FWarriorStageRecord& Record = Stats.GetCurrentStageRecord();
	const FWarriorAttackStats& Attack = Record.Stats.Attack;
	TestEqual(TEXT("Hits dealt"), Attack.HitsDealt, 3);
	TestEqual(TEXT("Damage dealt"), Attack.DamageDealt, 100.0);
	TestEqual(TEXT("Max damage"), Attack.MaxDamageDealt, 50.0);
	TestEqual(TEXT("Attacks landed"), Attack.AttacksLanded, 2);
	TestEqual(TEXT("Player kills"), Attack.Kills, 4);
	TestEqual(TEXT("Max kills per attack"), Attack.MaxKillsPerAttack, 2);
	TestEqual(TEXT("Kills by enemy type"), Attack.KillsByEnemyType.FindRef(WST::TestEnemyType), 4);
	TestEqual(TEXT("Kills by death Normal"), Attack.KillsByDeathType.FindRef(TEXT("Normal")), 2);
	TestEqual(TEXT("Kills by death Finisher"), Attack.KillsByDeathType.FindRef(TEXT("Finisher")), 1);
	TestEqual(TEXT("Kills by death Unknown"), Attack.KillsByDeathType.FindRef(TEXT("Unknown")), 1);

	int32 DeathTypeSum = 0;
	for (const TPair<FName, int32>& Pair : Attack.KillsByDeathType)
	{
		DeathTypeSum += Pair.Value;
	}
	TestEqual(TEXT("Death type sum matches kills"), DeathTypeSum, Attack.Kills);

	const FWarriorStatValue* Overkill = Record.Stats.FindExtra(WarriorStatTags::Stat_Combat_Damage_Overkill, WST::TestEnemyType);
	if (TestNotNull(TEXT("Overkill extra"), Overkill))
	{
		TestEqual(TEXT("Overkill sum"), Overkill->Sum, 10.0);
	}

	const FWarriorEnemyTypeStats* EnemyStats = Record.Enemies.FindByPredicate([](const FWarriorEnemyTypeStats& Candidate) { return Candidate.EnemyType == WST::TestEnemyType; });
	if (TestNotNull(TEXT("Enemy type stats"), EnemyStats))
	{
		TestEqual(TEXT("Enemy spawned"), EnemyStats->Spawned, 5);
		TestEqual(TEXT("Enemy killed (all causes)"), EnemyStats->Killed, 5);
	}
	if (TestEqual(TEXT("One wave"), Record.Waves.Num(), 1))
	{
		TestEqual(TEXT("Wave kills (all causes)"), Record.Waves[0].Kills, 5);
	}

	// 적 → 플레이어
	const FGameplayEffectContextHandle EnemyHit = WST::MakeContext(Enemies[0]);
	Stats.HandleDamage(EnemyHit, Player, 15.0f, 0.0f, false);
	Stats.HandleDamage(EnemyHit, Player, 25.0f, 0.0f, true);
	const FWarriorDefenseStats& Defense = Record.Stats.Defense;
	TestEqual(TEXT("Hits taken"), Defense.HitsTaken, 2);
	TestEqual(TEXT("Damage taken"), Defense.DamageTaken, 40.0);
	TestEqual(TEXT("Deaths"), Defense.Deaths, 1);
	TestEqual(TEXT("Damage taken by enemy"), Defense.DamageTakenByEnemyType.FindRef(WST::TestEnemyType), 40.0);
	if (TestEqual(TEXT("Death record"), Record.Deaths.Num(), 1))
	{
		TestTrue(TEXT("Killer type"), Record.Deaths[0].KillerEnemyType == WST::TestEnemyType);
		TestEqual(TEXT("Death wave"), Record.Deaths[0].WaveNumber, 1);
	}
	EnemyStats = Record.Enemies.FindByPredicate([](const FWarriorEnemyTypeStats& Candidate) { return Candidate.EnemyType == WST::TestEnemyType; });
	if (EnemyStats)
	{
		TestEqual(TEXT("Enemy damage to player"), EnemyStats->DamageToPlayer, 40.0);
		TestEqual(TEXT("Enemy player kills"), EnemyStats->PlayerKills, 1);
	}

	// 회복·그로기·경제
	Stats.HandleHeal(PlayerHit, Player, 30.0f, 5.0f);
	Stats.HandleHeal(PlayerHit, Enemies[0], 100.0f, 0.0f);
	Stats.HandleBalanceDamage(PlayerHit, Enemies[0], 12.0f);
	Stats.HandleBalanceDamage(EnemyHit, Enemies[1], 50.0f);
	Stats.HandleGoldEarned(6, TEXT("Kill"));
	Stats.HandleGoldEarned(4, NAME_None);
	Stats.HandleGoldEarned(-3, TEXT("Kill"));
	Stats.HandleGoldSpent(3);
	Stats.HandlePotionUsed(TEXT("Potion_S"));
	Stats.HandlePotionUsed(TEXT("Potion_S"));
	Stats.HandlePurchase(TEXT("Potion_S"), 2, 20);
	Stats.HandlePurchase(NAME_None, 1, 10);

	TestEqual(TEXT("Heal on player only"), Record.Stats.Heal.HealAmount, 30.0);
	TestEqual(TEXT("Overheal"), Record.Stats.Heal.Overheal, 5.0);
	const FWarriorStatValue* Balance = Record.Stats.FindExtra(WarriorStatTags::Stat_Combat_Balance_Dealt, WST::TestEnemyType);
	if (TestNotNull(TEXT("Balance extra"), Balance))
	{
		TestEqual(TEXT("Balance by player only"), Balance->Sum, 12.0);
	}
	const FWarriorEconomyStats& Economy = Record.Stats.Economy;
	TestEqual(TEXT("Gold earned (negative ignored)"), Economy.GoldEarned, 10);
	TestEqual(TEXT("Gold by Kill"), Economy.GoldEarnedBySource.FindRef(TEXT("Kill")), 6);
	TestEqual(TEXT("Gold by Unknown"), Economy.GoldEarnedBySource.FindRef(TEXT("Unknown")), 4);
	TestEqual(TEXT("Gold spent (purchase does not add)"), Economy.GoldSpent, 3);
	TestEqual(TEXT("Potions used"), Record.Stats.Heal.PotionsUsed, 2);
	TestEqual(TEXT("Potions by item"), Record.Stats.Heal.PotionsUsedByItem.FindRef(TEXT("Potion_S")), 2);
	TestEqual(TEXT("Purchase count"), Economy.PurchasesByItem.FindRef(TEXT("Potion_S")).Count, 2);
	TestEqual(TEXT("Purchase gold"), Economy.PurchasesByItem.FindRef(TEXT("Potion_S")).GoldSpent, 20);
	TestEqual(TEXT("Purchase history (item id required)"), Record.Purchases.Num(), 1);
	return true;
}

//~ 웨이브 기록

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsWaveRecordsTest, "ProjectWarrior.Stats.S1.WaveRecords", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsWaveRecordsTest::RunTest(const FString& Parameters)
{
	WST::FStageTestWorld Test;
	if (!TestNotNull(TEXT("World"), Test.World) || !TestNotNull(TEXT("Stage stats"), Test.Stats))
	{
		return false;
	}
	UWarriorStageStatsSubsystem& Stats = *Test.Stats;
	FWarriorStatsStageTest::BeginFakeStage(Stats);
	const FWarriorStageRecord& Record = Stats.GetCurrentStageRecord();

	// 0번(웨이브 전)은 열지 않는다.
	FWarriorStatsStageTest::ChangeWave(Stats, 0, 4, false);
	TestEqual(TEXT("Wave 0 is not opened"), Record.Waves.Num(), 0);
	TestEqual(TEXT("Total waves from broadcast"), Record.TotalWaves, 4);

	// 웨이브 1: 맞지 않고 클리어 → 무피격 1회
	FWarriorStatsStageTest::ChangeWave(Stats, 1, 4, false);
	FWarriorStatsStageTest::ChangeWave(Stats, 1, 4, false);
	TestEqual(TEXT("Same wave number is not opened twice"), Record.Waves.Num(), 1);
	TestEqual(TEXT("Current wave"), Stats.GetCurrentWaveNumber(), 1);
	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::WaveCleared);
	TestEqual(TEXT("Waves cleared after wave 1"), Record.WavesCleared, 1);
	TestTrue(TEXT("Wave 1 cleared"), Record.Waves[0].bCleared);
	TestNull(TEXT("Cleared wave is not writable"), Stats.GetMutableCurrentWave());
	const FWarriorStatValue* NoHit = Record.Stats.FindExtra(WarriorStatTags::Stat_Defense_Wave_NoHit);
	if (TestNotNull(TEXT("No-hit extra"), NoHit))
	{
		TestEqual(TEXT("No-hit waves after wave 1"), NoHit->Count, 1);
	}

	// 같은 상태가 다시 와도 두 번 세지 않는다.
	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::WaveCleared);
	TestEqual(TEXT("Cleared wave is not counted twice"), Record.WavesCleared, 1);

	// 웨이브 2 (보스): 맞고 클리어 → 무피격 그대로
	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::Resting);
	FWarriorStatsStageTest::ChangeWave(Stats, 2, 4, true);
	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::InProgress);
	Stats.MarkPlayerHitThisWave();
	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::WaveCleared);
	TestTrue(TEXT("Wave 2 is boss"), Record.Waves[1].bBossWave);
	TestEqual(TEXT("Waves cleared after wave 2"), Record.WavesCleared, 2);
	NoHit = Record.Stats.FindExtra(WarriorStatTags::Stat_Defense_Wave_NoHit);
	if (NoHit)
	{
		TestEqual(TEXT("Hit wave does not add no-hit"), NoHit->Count, 1);
	}

	// 웨이브 3이 클리어 전에 4로 넘어가면 3은 미클리어로 닫힌다.
	FWarriorStatsStageTest::ChangeWave(Stats, 3, 4, false);
	FWarriorStatsStageTest::ChangeWave(Stats, 4, 4, false);
	TestFalse(TEXT("Skipped wave 3 is not cleared"), Record.Waves[2].bCleared);
	TestEqual(TEXT("Skipped wave is not counted"), Record.WavesCleared, 2);

	// 스테이지 클리어: 마지막 웨이브를 닫고 한 번만 넘긴다. 테스트 월드에는 GameInstance가 없어 보관소 경고가 난다.
	AddExpectedMessage(TEXT("No profile stats subsystem"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::StageCleared);
	TestTrue(TEXT("Wave 4 closed as cleared"), Record.Waves[3].bCleared);
	TestEqual(TEXT("Waves cleared at the end"), Record.WavesCleared, 3);
	TestTrue(TEXT("Outcome"), Record.Outcome == EWarriorStatOutcome::Cleared);
	TestTrue(TEXT("Record submitted"), FWarriorStatsStageTest::IsSubmitted(Stats));
	TestFalse(TEXT("Not recording after finish"), Stats.IsRecording());

	FWarriorStatsStageTest::ChangeState(Stats, EWarriorStageState::StageFailed);
	TestTrue(TEXT("Finished record is not changed"), Record.Outcome == EWarriorStatOutcome::Cleared);
	return true;
}

//~ 스테이지 → 판·누적

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsStageToRunTest, "ProjectWarrior.Stats.S1.StageToRun", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsStageToRunTest::RunTest(const FString& Parameters)
{
	UWarriorProfileStatsSubsystem* Profile = WST::MakeProfile();
	if (!TestNotNull(TEXT("Profile"), Profile))
	{
		return false;
	}
	Profile->SetEndRunOnStageCleared(false);

	// 진행 중인 기록은 받지 않는다.
	AddExpectedMessage(TEXT("still in progress"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Profile->AddStageRecord(WST::MakeStageRecord(EWarriorStatOutcome::InProgress, 1, 1, 1.0, 1));
	TestEqual(TEXT("In-progress record rejected"), Profile->GetStageRecords().Num(), 0);
	TestFalse(TEXT("Rejected record does not start a run"), Profile->HasActiveRun());

	// 스테이지 1 클리어 (판은 계속)
	Profile->AddStageRecord(WST::MakeStageRecord(EWarriorStatOutcome::Cleared, 5, 30, 60.0, 3));
	TestTrue(TEXT("Run started by first stage"), Profile->HasActiveRun());
	const FGuid RunId = Profile->GetCurrentRun().RunId;
	FWarriorStageRecord LastStage;
	if (TestTrue(TEXT("Last stage record"), Profile->GetLastStageRecord(LastStage)))
	{
		TestTrue(TEXT("Stage attached to current run"), LastStage.RunId == RunId);
	}

	// 스테이지 2 클리어: 옛 판 ID를 달고 와도 현재 판에 붙는다.
	FWarriorStageRecord Stage2 = WST::MakeStageRecord(EWarriorStatOutcome::Cleared, 7, 50, 45.0, 5);
	Stage2.RunId = FGuid::NewGuid();
	AddExpectedMessage(TEXT("no longer active"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Profile->AddStageRecord(Stage2);
	Profile->GetLastStageRecord(LastStage);
	TestTrue(TEXT("Stale run id reattached"), LastStage.RunId == RunId);

	const FWarriorRunRecord& Run = Profile->GetCurrentRun();
	TestEqual(TEXT("Run stages played"), Run.StagesPlayed, 2);
	TestEqual(TEXT("Run stages cleared"), Run.StagesCleared, 2);
	TestEqual(TEXT("Run kills"), Run.Stats.Attack.Kills, 12);
	TestEqual(TEXT("Run gold"), Run.Stats.Economy.GoldEarned, 80);
	TestEqual(TEXT("Run play time"), Run.Stats.PlayTimeSeconds, 105.0);
	TestTrue(TEXT("Run stage ids"), Run.StageRecordIds.Num() == 2);

	const FWarriorLifetimeStats& Lifetime = Profile->GetLifetimeStats();
	TestEqual(TEXT("Lifetime kills"), Lifetime.Stats.Attack.Kills, 12);
	TestEqual(TEXT("Lifetime stages cleared"), Lifetime.StagesCleared, 2);
	TestEqual(TEXT("Best clear time"), Lifetime.BestStageClearTimeSeconds, 45.0f);
	TestEqual(TEXT("Max wave reached"), Lifetime.MaxWaveReached, 5);
	TestEqual(TEXT("Lifetime enemy kills"), Lifetime.EnemiesByType.FindRef(WST::TestEnemyType).Killed, 12);

	// 중도 이탈: 판에는 합산하고 판을 끝낸다. 누적에는 플레이 수만 반영한다.
	Profile->AddStageRecord(WST::MakeStageRecord(EWarriorStatOutcome::Abandoned, 3, 10, 20.0, 7));
	TestFalse(TEXT("Abandoned stage ends the run"), Profile->HasActiveRun());
	FWarriorRunRecord LastRun;
	if (TestTrue(TEXT("Last run record"), Profile->GetLastRunRecord(LastRun)))
	{
		TestTrue(TEXT("Run outcome"), LastRun.Outcome == EWarriorStatOutcome::Abandoned);
		TestEqual(TEXT("Abandoned stage counted in run"), LastRun.Stats.Attack.Kills, 15);
		TestEqual(TEXT("Run stages played with abandoned"), LastRun.StagesPlayed, 3);
	}
	TestEqual(TEXT("Lifetime stages played includes abandoned"), Lifetime.StagesPlayed, 3);
	TestEqual(TEXT("Lifetime kills exclude abandoned"), Lifetime.Stats.Attack.Kills, 12);
	TestEqual(TEXT("Max wave excludes abandoned"), Lifetime.MaxWaveReached, 5);
	TestEqual(TEXT("Lifetime runs played"), Lifetime.RunsPlayed, 1);
	TestEqual(TEXT("Lifetime runs cleared"), Lifetime.RunsCleared, 0);
	return true;
}

//~ 판 시작·종료

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsRunLifecycleTest, "ProjectWarrior.Stats.S1.RunLifecycle", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsRunLifecycleTest::RunTest(const FString& Parameters)
{
	UWarriorProfileStatsSubsystem* Profile = WST::MakeProfile();
	if (!TestNotNull(TEXT("Profile"), Profile))
	{
		return false;
	}

	// 판이 없을 때 EndRun은 아무것도 하지 않는다.
	Profile->EndRun(EWarriorStatOutcome::Cleared);
	TestEqual(TEXT("No run to end"), Profile->GetRunRecords().Num(), 0);

	// BeginRunIfNeeded는 진행 중인 판을 그대로 돌려준다.
	const FGuid RunId = Profile->BeginRunIfNeeded();
	TestTrue(TEXT("Run id valid"), RunId.IsValid());
	TestTrue(TEXT("Same run while active"), Profile->BeginRunIfNeeded() == RunId);

	// InProgress로는 끝낼 수 없다.
	AddExpectedMessage(TEXT("must not be InProgress"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Profile->EndRun(EWarriorStatOutcome::InProgress);
	TestTrue(TEXT("Run still active"), Profile->HasActiveRun());

	// 기본 규칙(D9): 스테이지 클리어 시 판도 클리어로 끝난다.
	Profile->AddStageRecord(WST::MakeStageRecord(EWarriorStatOutcome::Cleared, 2, 10, 30.0, 2));
	TestFalse(TEXT("Cleared stage ends the run by default"), Profile->HasActiveRun());
	FWarriorRunRecord LastRun;
	if (TestTrue(TEXT("Run stored"), Profile->GetLastRunRecord(LastRun)))
	{
		TestTrue(TEXT("Run id kept"), LastRun.RunId == RunId);
		TestTrue(TEXT("Run cleared"), LastRun.Outcome == EWarriorStatOutcome::Cleared);
	}
	TestEqual(TEXT("Runs cleared"), Profile->GetLifetimeStats().RunsCleared, 1);

	// 실패는 규칙과 상관없이 판을 끝낸다.
	Profile->SetEndRunOnStageCleared(false);
	Profile->AddStageRecord(WST::MakeStageRecord(EWarriorStatOutcome::Cleared, 1, 5, 10.0, 1));
	TestTrue(TEXT("Run continues when rule is off"), Profile->HasActiveRun());
	const FGuid SecondRunId = Profile->GetCurrentRun().RunId;
	TestTrue(TEXT("New run after previous ended"), SecondRunId != RunId);
	Profile->AddStageRecord(WST::MakeStageRecord(EWarriorStatOutcome::Failed, 0, 0, 10.0, 1));
	TestFalse(TEXT("Failed stage ends the run"), Profile->HasActiveRun());
	Profile->GetLastRunRecord(LastRun);
	TestTrue(TEXT("Second run failed"), LastRun.Outcome == EWarriorStatOutcome::Failed);
	TestEqual(TEXT("Second run stages"), LastRun.StagesPlayed, 2);
	TestEqual(TEXT("Stages failed"), Profile->GetLifetimeStats().StagesFailed, 1);
	TestEqual(TEXT("Runs played"), Profile->GetLifetimeStats().RunsPlayed, 2);

	// 전체 초기화
	Profile->ResetAll();
	TestEqual(TEXT("Reset stage records"), Profile->GetStageRecords().Num(), 0);
	TestEqual(TEXT("Reset run records"), Profile->GetRunRecords().Num(), 0);
	TestEqual(TEXT("Reset lifetime"), Profile->GetLifetimeStats().StagesPlayed, 0);
	TestFalse(TEXT("Reset active run"), Profile->HasActiveRun());
	return true;
}

//~ 보관 상한

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsRecordLimitsTest, "ProjectWarrior.Stats.S1.RecordLimits", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsRecordLimitsTest::RunTest(const FString& Parameters)
{
	UWarriorProfileStatsSubsystem* Profile = WST::MakeProfile();
	if (!TestNotNull(TEXT("Profile"), Profile))
	{
		return false;
	}

	// 클리어마다 판이 끝나므로 스테이지 1개 = 판 1개
	const int32 Total = UWarriorProfileStatsSubsystem::MaxStageRecords + 5;
	FGuid FirstKeptRecordId;
	for (int32 Index = 0; Index < Total; ++Index)
	{
		FWarriorStageRecord Record = WST::MakeStageRecord(EWarriorStatOutcome::Cleared, 1, 1, 10.0, 1);
		if (Index == Total - UWarriorProfileStatsSubsystem::MaxStageRecords)
		{
			FirstKeptRecordId = Record.RecordId;
		}
		Profile->AddStageRecord(Record);
	}

	TestEqual(TEXT("Stage records capped"), Profile->GetStageRecords().Num(), UWarriorProfileStatsSubsystem::MaxStageRecords);
	TestEqual(TEXT("Run records capped"), Profile->GetRunRecords().Num(), UWarriorProfileStatsSubsystem::MaxRunRecords);
	TestTrue(TEXT("Oldest stage records removed first"), Profile->GetStageRecords()[0].RecordId == FirstKeptRecordId);
	TestEqual(TEXT("Lifetime keeps every stage"), Profile->GetLifetimeStats().StagesPlayed, Total);
	TestEqual(TEXT("Lifetime keeps every kill"), Profile->GetLifetimeStats().Stats.Attack.Kills, Total);
	TestEqual(TEXT("Lifetime keeps every run"), Profile->GetLifetimeStats().RunsPlayed, Total);
	return true;
}

//~ 스테이지 밖에서는 기록하지 않음

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsIgnoredOutsideStageTest, "ProjectWarrior.Stats.S1.IgnoredOutsideStage", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsIgnoredOutsideStageTest::RunTest(const FString& Parameters)
{
	WST::FStageTestWorld Test;
	if (!TestNotNull(TEXT("World"), Test.World) || !TestNotNull(TEXT("Stage stats"), Test.Stats))
	{
		return false;
	}
	UWarriorStageStatsSubsystem& Stats = *Test.Stats;
	APawn* Player = Test.SpawnPlayer();
	AWarriorAICharacter* Enemy = Test.Spawn<AWarriorAICharacter>(FVector(0.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Enemy"), Enemy))
	{
		return false;
	}

	// GameMode가 스테이지가 아닌 월드: 기록을 열지 않는다.
	TestFalse(TEXT("Not recording without a stage game mode"), Stats.IsRecording());
	TestNull(TEXT("No mutable record"), Stats.GetMutableRecord());
	TestNull(TEXT("No mutable stats"), Stats.GetMutableStats());

	const FGameplayEffectContextHandle PlayerHit = WST::MakeContext(Player);
	Stats.HandleEnemySpawned(Enemy);
	Stats.HandleDamage(PlayerHit, Enemy, 30.0f, 0.0f, true);
	Stats.HandleEnemyKilled(Enemy, TEXT("Normal"));
	Stats.HandleHeal(PlayerHit, Player, 10.0f, 0.0f);
	Stats.HandleGoldEarned(10, TEXT("Kill"));
	Stats.HandleGoldSpent(5);
	Stats.HandlePotionUsed(TEXT("Potion_S"));
	Stats.HandlePurchase(TEXT("Potion_S"), 1, 10);
	Stats.HandleStat(WarriorStatTags::Stat_Time_Shop, 3.0, NAME_None);

	// 라이브러리 진입점도 같은 조건으로 무시한다.
	UWarriorStatsLibrary::RecordEnemySpawned(Enemy);
	UWarriorStatsLibrary::RecordDamage(PlayerHit, Enemy, 30.0f, 0.0f, true);
	UWarriorStatsLibrary::RecordEnemyKilled(Enemy, TEXT("Normal"));
	UWarriorStatsLibrary::RecordGoldEarned(Test.World, 10, TEXT("Kill"));
	UWarriorStatsLibrary::RecordPurchase(Test.World, TEXT("Potion_S"), 1, 10);

	const FWarriorStageRecord& Record = Stats.GetCurrentStageRecord();
	TestEqual(TEXT("No kills"), Record.Stats.Attack.Kills, 0);
	TestEqual(TEXT("No hits"), Record.Stats.Attack.HitsDealt, 0);
	TestEqual(TEXT("No heal"), Record.Stats.Heal.HealAmount, 0.0);
	TestEqual(TEXT("No gold earned"), Record.Stats.Economy.GoldEarned, 0);
	TestEqual(TEXT("No gold spent"), Record.Stats.Economy.GoldSpent, 0);
	TestEqual(TEXT("No potions"), Record.Stats.Heal.PotionsUsed, 0);
	TestEqual(TEXT("No purchases"), Record.Purchases.Num(), 0);
	TestEqual(TEXT("No extras"), Record.Stats.Extra.Num(), 0);
	TestEqual(TEXT("No enemies"), Record.Enemies.Num(), 0);
	TestEqual(TEXT("Live play time outside stage"), Stats.GetLivePlayTimeSeconds(), 0.0f);
	TestEqual(TEXT("No watched gold"), Stats.GetCurrentGold(), -1);
	return true;
}

//~ 게임 종료 시 중도 이탈

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStatsAbandonOnShutdownTest, "ProjectWarrior.Stats.S1.AbandonOnShutdown", WARRIOR_STATS_TEST_FLAGS)

bool FWarriorStatsAbandonOnShutdownTest::RunTest(const FString& Parameters)
{
	// PIE 종료 시 GameInstance가 월드보다 먼저 정리된다. 보관소가 진행 중인 스테이지를 직접 받아 판을 끝내는지 확인한다.
	WST::FStageTestWorld Test;
	UWarriorProfileStatsSubsystem* Profile = WST::MakeProfile();
	if (!TestNotNull(TEXT("World"), Test.World) || !TestNotNull(TEXT("Stage stats"), Test.Stats) || !TestNotNull(TEXT("Profile"), Profile))
	{
		return false;
	}
	UWarriorStageStatsSubsystem& Stats = *Test.Stats;

	// 기록 중이 아니면 아무것도 넘기지 않는다.
	Stats.FinishAsAbandoned(Profile);
	TestEqual(TEXT("Nothing submitted while not recording"), Profile->GetStageRecords().Num(), 0);

	FWarriorStatsStageTest::BeginFakeStage(Stats);
	Stats.HandleGoldEarned(5, TEXT("Kill"));
	Stats.FinishAsAbandoned(Profile);

	FWarriorStageRecord LastStage;
	if (TestTrue(TEXT("Abandoned stage stored"), Profile->GetLastStageRecord(LastStage)))
	{
		TestTrue(TEXT("Stage outcome"), LastStage.Outcome == EWarriorStatOutcome::Abandoned);
		TestEqual(TEXT("Stage stats kept"), LastStage.Stats.Economy.GoldEarned, 5);
	}
	FWarriorRunRecord LastRun;
	if (TestTrue(TEXT("Run ended"), Profile->GetLastRunRecord(LastRun)))
	{
		TestTrue(TEXT("Run outcome"), LastRun.Outcome == EWarriorStatOutcome::Abandoned);
		TestEqual(TEXT("Run includes the stage"), LastRun.StagesPlayed, 1);
	}
	TestFalse(TEXT("Not recording after abandon"), Stats.IsRecording());

	// 두 번 불려도 한 번만 넘긴다.
	Stats.FinishAsAbandoned(Profile);
	TestEqual(TEXT("Submitted once"), Profile->GetStageRecords().Num(), 1);
	return true;
}

#undef WARRIOR_STATS_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
