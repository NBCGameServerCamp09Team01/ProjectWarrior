#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Stage/WarriorWaveSpawner.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorWaveSpawnerTest, "ProjectWarrior.Stage.W1.SpawnerLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorWaveSpawnerTest::RunTest(const FString& Parameters)
{
	// 별도 월드에서 BeginPlay를 호출하지 않고, AI 초기 데이터나 제작된 맵 없이 스폰과 집계를 검증한다.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
	AWarriorWaveSpawner* Spawner = World->SpawnActor<AWarriorWaveSpawner>();
	if (!TestNotNull(TEXT("Spawner"), Spawner)) { World->DestroyWorld(false); return false; }
	int32 ClearCount = 0;
	int32 LatestAlive = -1;
	int32 LatestSpawned = -1;
	Spawner->OnWaveCleared.AddLambda([&]() { ++ClearCount; });
	Spawner->OnEnemyCountChanged.AddLambda([&](int32 Alive, int32 Spawned)
	{
		LatestAlive = Alive;
		LatestSpawned = Spawned;
	});

	FWarriorStageWaveData Wave;
	FWarriorWaveEnemySpawnData Entry;
	Entry.EnemyClass = AWarriorAICharacter::StaticClass();
	Entry.Count = 2;
	Entry.SpawnGroup = TEXT("Regular");
	Wave.Enemies.Add(Entry);

	// 빈 웨이브를 거절할 때 클리어 이벤트가 발생하면 안 된다.
	AddExpectedError(TEXT("rejected empty or invalid wave data"), EAutomationExpectedErrorFlags::Contains, 2);
	TestFalse(TEXT("Empty wave is rejected"), Spawner->StartWaveFromData(FWarriorStageWaveData()));
	TestEqual(TEXT("Empty wave does not clear"), ClearCount, 0);

	// 적 수량이 잘못된 데이터도 시작을 거절한다.
	FWarriorStageWaveData InvalidWave = Wave;
	InvalidWave.Enemies[0].Count = 0;
	TestFalse(TEXT("Invalid wave is rejected"), Spawner->StartWaveFromData(InvalidWave));
	TestEqual(TEXT("Invalid wave does not clear"), ClearCount, 0);

	// 스폰 그룹이 없으면 한도까지 재시도한 뒤 요청을 건너뛰고, 모든 요청을 건너뛰면 클리어한다.
	Spawner->MaxSpawnAttempts = 2;
	TestTrue(TEXT("Valid wave is accepted"), Spawner->StartWaveFromData(Wave));
	TestFalse(TEXT("Overlapping wave is rejected"), Spawner->StartWaveFromData(Wave));
	TestEqual(TEXT("Rejected overlapping wave does not clear"), ClearCount, 0);
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Failed request not consumed"), Spawner->NextRequestIndex, 0);
	TestTrue(TEXT("Pending request counts as remaining work"), Spawner->HasAliveEnemies());
	AddExpectedError(TEXT("skipped request"), EAutomationExpectedErrorFlags::Contains, 3);
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Request skipped at limit"), Spawner->NextRequestIndex, 1);
	TestEqual(TEXT("Remaining spawn count after skip"), Spawner->GetRemainingSpawnCount(), 1);
	TestEqual(TEXT("Retry count resets after skip"), Spawner->CurrentSpawnAttempts, 0);
	TestEqual(TEXT("Pending request prevents clear after skip"), ClearCount, 0);
	TestTrue(TEXT("Next request timer continues after skip"), World->GetTimerManager().IsTimerActive(Spawner->SpawnTimerHandle));
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Next request gets its own retry budget"), Spawner->NextRequestIndex, 1);
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("All requests skipped"), Spawner->NextRequestIndex, 2);
	TestEqual(TEXT("No remaining spawn requests"), Spawner->GetRemainingSpawnCount(), 0);
	Spawner->TryReportWaveCleared();
	TestEqual(TEXT("Wave with only skipped requests clears once"), ClearCount, 1);
	TestFalse(TEXT("No remaining work after skips"), Spawner->HasAliveEnemies());
	TestEqual(TEXT("Skipped requests are not counted as spawned"), LatestSpawned, 0);
	Spawner->StopSpawning();
	TestFalse(TEXT("Stop cancels pending work"), Spawner->HasAliveEnemies());
	ClearCount = 0;

	ATargetPoint* Point = World->SpawnActor<ATargetPoint>();
	Point->SetActorLocation(FVector(0, 0, 300));
	FWarriorWaveSpawnPointData PointData;
	PointData.SpawnPoint = Point;
	PointData.SpawnGroup = TEXT("Regular");
	Spawner->SpawnPoints.Add(PointData);
	Spawner->StartWaveFromData(Wave);
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("One enemy spawned"), LatestSpawned, 1);
	TestEqual(TEXT("One enemy alive"), LatestAlive, 1);
	if (Spawner->AliveEnemies.Num() == 1)
	{
		AWarriorAICharacter* First = Spawner->AliveEnemies.Array()[0].Get();
		First->Destroy();
		Spawner->HandleSpawnedEnemyDestroyed(First); // 중복 알림이 와도 집계 결과가 달라지면 안 된다.
		TestEqual(TEXT("Destroyed enemy removed once"), Spawner->GetAliveEnemyCount(), 0);
		TestEqual(TEXT("Pending second enemy prevents early clear"), ClearCount, 0);
	}
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Second request consumed"), Spawner->NextRequestIndex, 2);
	TestEqual(TEXT("Cumulative successful spawns"), LatestSpawned, 2);
	if (Spawner->AliveEnemies.Num() == 1)
	{
		AWarriorAICharacter* Last = Spawner->AliveEnemies.Array()[0].Get();
		Last->Destroy();
		Spawner->HandleSpawnedEnemyDestroyed(Last);
	}
	Spawner->TryReportWaveCleared();
	TestEqual(TEXT("Exactly one wave completion"), ClearCount, 1);
	TestEqual(TEXT("Final alive count published"), LatestAlive, 0);

	// 일부 요청을 건너뛰더라도 이미 생성된 적이 남아 있으면 클리어하면 안 된다.
	Spawner->StopSpawning();
	ClearCount = 0;
	FWarriorStageWaveData MixedWave;
	FWarriorWaveEnemySpawnData MixedEntry = Entry;
	MixedEntry.Count = 1;
	MixedWave.Enemies.Add(MixedEntry);
	MixedEntry.SpawnGroup = TEXT("Missing");
	MixedWave.Enemies.Add(MixedEntry);
	TestTrue(TEXT("Mixed wave starts"), Spawner->StartWaveFromData(MixedWave));
	Spawner->ProcessNextSpawnRequest();
	Spawner->ProcessNextSpawnRequest();
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Mixed wave has no pending requests"), Spawner->GetRemainingSpawnCount(), 0);
	TestEqual(TEXT("Mixed wave counts only successful spawns"), LatestSpawned, 1);
	TestEqual(TEXT("Mixed wave keeps spawned enemy alive"), LatestAlive, 1);
	TestEqual(TEXT("Skipped last request waits for living enemy"), ClearCount, 0);
	if (Spawner->AliveEnemies.Num() == 1)
	{
		AWarriorAICharacter* RemainingEnemy = Spawner->AliveEnemies.Array()[0].Get();
		RemainingEnemy->Destroy();
		Spawner->HandleSpawnedEnemyDestroyed(RemainingEnemy);
	}
	Spawner->TryReportWaveCleared();
	TestEqual(TEXT("Mixed wave clears once after enemy removal"), ClearCount, 1);

	// 이벤트 수신 중 StopSpawning을 호출했다면, 이후 첫 스폰 타이머가 등록되면 안 된다.
	Spawner->OnEnemyCountChanged.AddLambda([Spawner](int32, int32) { Spawner->StopSpawning(); });
	Spawner->StartWaveFromData(Wave);
	TestFalse(TEXT("Stop from notification cancels work"), Spawner->HasAliveEnemies());
	TestFalse(TEXT("Stop from notification leaves no timer"), World->GetTimerManager().TimerExists(Spawner->SpawnTimerHandle));
	Spawner->OnEnemyCountChanged.Clear();
	Spawner->OnWaveCleared.Clear();
	Spawner->StopSpawning();
	World->DestroyWorld(false);
	return true;
}

#endif
