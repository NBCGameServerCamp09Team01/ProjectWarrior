#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Stage/WarriorWaveSpawner.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorWaveSpawnerDeathRewardTest, "ProjectWarrior.Stage.W3.DeathAndReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorWaveSpawnerDeathRewardTest::RunTest(const FString& Parameters)
{
	// 사망 신호 집계와 골드 보상 계산을 검증한다. 테스트 월드에는 PlayerState가 없으므로 지급은 OnEnemyRewarded로 확인한다.
	// 초기화하지 않은 테스트 월드에서는 AActor::ProcessEvent가 막혀 다이나믹 델리게이트 방송이 전달되지 않고,
	// Destroy도 월드 컨텍스트가 없어 실패한다. 그래서 구독 여부는 IsAlreadyBound로 확인하고, 신호는 핸들러를 직접 호출한다.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
	AWarriorWaveSpawner* Spawner = World->SpawnActor<AWarriorWaveSpawner>();
	if (!TestNotNull(TEXT("Spawner"), Spawner)) { World->DestroyWorld(false); return false; }

	// 적끼리 겹치지 않도록 스폰 포인트를 떨어뜨려 둔다.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ATargetPoint* Point = World->SpawnActor<ATargetPoint>();
		Point->SetActorLocation(FVector(Index * 1000.0, 0, 300));
		FWarriorWaveSpawnPointData PointData;
		PointData.SpawnPoint = Point;
		Spawner->SpawnPoints.Add(PointData);
	}

	int32 ClearCount = 0;
	int32 LatestAlive = -1;
	int32 RewardCount = 0;
	int32 RewardTotal = 0;
	Spawner->OnWaveCleared.AddLambda([&]() { ++ClearCount; });
	Spawner->OnEnemyCountChanged.AddLambda([&](int32 Alive, int32) { LatestAlive = Alive; });
	Spawner->OnEnemyRewarded.AddLambda([&](AWarriorAICharacter*, int32 Gold)
	{
		++RewardCount;
		RewardTotal += Gold;
	});
	auto ResetCounters = [&]()
	{
		ClearCount = 0;
		LatestAlive = -1;
		RewardCount = 0;
		RewardTotal = 0;
	};

	auto MakeWave = [](int32 Count, int32 GoldReward, float GoldDropChance)
	{
		FWarriorStageWaveData Wave;
		FWarriorWaveEnemySpawnData Entry;
		Entry.EnemyClass = AWarriorAICharacter::StaticClass();
		Entry.Count = Count;
		Entry.GoldReward = GoldReward;
		Entry.GoldDropChance = GoldDropChance;
		Wave.Enemies.Add(Entry);
		return Wave;
	};

	// 웨이브를 시작하고 모든 요청을 스폰한 뒤 살아 있는 적 목록을 돌려준다.
	auto StartAndSpawnAll = [&](const FWarriorStageWaveData& Wave)
	{
		TArray<AWarriorAICharacter*> Enemies;
		if (!TestTrue(TEXT("Wave starts"), Spawner->StartWaveFromData(Wave)))
		{
			return Enemies;
		}
		for (int32 Step = 0; Step < 20 && Spawner->GetRemainingSpawnCount() > 0; ++Step)
		{
			Spawner->ProcessNextSpawnRequest();
		}
		for (const TWeakObjectPtr<AWarriorAICharacter>& Enemy : Spawner->AliveEnemies)
		{
			if (Enemy.IsValid())
			{
				Enemies.Add(Enemy.Get());
			}
		}
		return Enemies;
	};

	// 실제 게임에서 OnCharacterDied 방송이 부르는 핸들러를 직접 호출한다. (구독 여부는 아래에서 IsAlreadyBound로 확인)
	auto SimulateDeath = [&](AWarriorAICharacter* Enemy)
	{
		Spawner->HandleSpawnedEnemyDied(Enemy);
	};

	// 사망 신호 → 생존 수 감소, 골드는 배율 적용, 클리어는 정확히 1회
	{
		ResetCounters();
		FWarriorStageWaveData Wave = MakeWave(2, 10, 1.0f);
		Wave.DropModifier.GoldAmountMultiplier = 1.5f;
		TArray<AWarriorAICharacter*> Enemies = StartAndSpawnAll(Wave);
		if (TestEqual(TEXT("Two enemies spawned"), Enemies.Num(), 2))
		{
			TestTrue(TEXT("Spawned enemy subscribes to OnCharacterDied"),
				Enemies[0]->OnCharacterDied.IsAlreadyBound(Spawner, &AWarriorWaveSpawner::HandleSpawnedEnemyDied));
			TestTrue(TEXT("Spawned enemy subscribes to OnDestroyed"),
				Enemies[0]->OnDestroyed.IsAlreadyBound(Spawner, &AWarriorWaveSpawner::HandleSpawnedEnemyDestroyed));

			SimulateDeath(Enemies[0]);
			TestFalse(TEXT("Died enemy is unbound from OnCharacterDied"),
				Enemies[0]->OnCharacterDied.IsAlreadyBound(Spawner, &AWarriorWaveSpawner::HandleSpawnedEnemyDied));
			TestFalse(TEXT("Died enemy is unbound from OnDestroyed"),
				Enemies[0]->OnDestroyed.IsAlreadyBound(Spawner, &AWarriorWaveSpawner::HandleSpawnedEnemyDestroyed));
			TestEqual(TEXT("Died enemy leaves alive count"), Spawner->GetAliveEnemyCount(), 1);
			TestEqual(TEXT("Published alive count after death"), LatestAlive, 1);
			TestEqual(TEXT("First death rewards once"), RewardCount, 1);
			TestEqual(TEXT("Amount multiplier applied (10 x 1.5)"), RewardTotal, 15);
			TestEqual(TEXT("Living enemy prevents clear"), ClearCount, 0);

			// 같은 적의 중복 사망 신호와 이후 Destroy는 집계·보상을 바꾸지 않는다.
			SimulateDeath(Enemies[0]);
			Spawner->HandleSpawnedEnemyDestroyed(Enemies[0]);
			TestEqual(TEXT("Duplicate death and destroy ignored (alive)"), Spawner->GetAliveEnemyCount(), 1);
			TestEqual(TEXT("Duplicate death and destroy ignored (reward)"), RewardCount, 1);

			SimulateDeath(Enemies[1]);
			TestEqual(TEXT("Second death rewards"), RewardCount, 2);
			TestEqual(TEXT("Total gold"), RewardTotal, 30);
			TestEqual(TEXT("Wave clears once after last death"), ClearCount, 1);
			TestEqual(TEXT("Final alive count"), LatestAlive, 0);
		}
	}

	// 확률 0이면 보상 없음 (적 데이터 확률 0 / 웨이브 확률 배율 0)
	{
		ResetCounters();
		TArray<AWarriorAICharacter*> Enemies = StartAndSpawnAll(MakeWave(1, 10, 0.0f));
		if (TestEqual(TEXT("Zero-chance enemy spawned"), Enemies.Num(), 1))
		{
			SimulateDeath(Enemies[0]);
			TestEqual(TEXT("Zero drop chance gives no gold"), RewardCount, 0);
			TestEqual(TEXT("Zero-chance wave still clears"), ClearCount, 1);
		}

		ResetCounters();
		FWarriorStageWaveData Wave = MakeWave(1, 10, 1.0f);
		Wave.DropModifier.GoldDropChanceMultiplier = 0.0f;
		Enemies = StartAndSpawnAll(Wave);
		if (TestEqual(TEXT("Zero-multiplier enemy spawned"), Enemies.Num(), 1))
		{
			SimulateDeath(Enemies[0]);
			TestEqual(TEXT("Zero chance multiplier gives no gold"), RewardCount, 0);
		}
	}

	// GoldReward 0이면 보상 없음
	{
		ResetCounters();
		TArray<AWarriorAICharacter*> Enemies = StartAndSpawnAll(MakeWave(1, 0, 1.0f));
		if (TestEqual(TEXT("No-gold enemy spawned"), Enemies.Num(), 1))
		{
			SimulateDeath(Enemies[0]);
			TestEqual(TEXT("Zero gold reward gives nothing"), RewardCount, 0);
		}
	}

	// 사망 신호 없이 Destroy만 되면 집계는 되지만 보상은 없다.
	{
		ResetCounters();
		TArray<AWarriorAICharacter*> Enemies = StartAndSpawnAll(MakeWave(1, 10, 1.0f));
		if (TestEqual(TEXT("Destroy-only enemy spawned"), Enemies.Num(), 1))
		{
			Spawner->HandleSpawnedEnemyDestroyed(Enemies[0]);
			TestEqual(TEXT("Destroy-only enemy leaves alive count"), Spawner->GetAliveEnemyCount(), 0);
			TestEqual(TEXT("Destroy-only enemy gives no gold"), RewardCount, 0);
			TestEqual(TEXT("Destroy-only wave clears once"), ClearCount, 1);
			TestEqual(TEXT("Reward entries cleaned up"), Spawner->EnemyRewards.Num(), 0);
		}
	}

	Spawner->OnEnemyRewarded.Clear();
	Spawner->OnEnemyCountChanged.Clear();
	Spawner->OnWaveCleared.Clear();
	Spawner->StopSpawning();
	World->DestroyWorld(false);
	return true;
}

#endif
