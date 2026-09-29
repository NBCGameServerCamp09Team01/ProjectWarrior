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
	AddExpectedError(TEXT("rejected empty or invalid wave data"), EAutomationExpectedErrorFlags::Contains, 1);
	Spawner->StartWaveFromData(FWarriorStageWaveData());
	TestEqual(TEXT("Empty wave does not clear"), ClearCount, 0);

	// 스폰 그룹이 없으면 재시도 중에도, 재시도 한도에 도달한 뒤에도 현재 요청을 유지해야 한다.
	Spawner->MaxSpawnAttempts = 2;
	Spawner->StartWaveFromData(Wave);
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Failed request not consumed"), Spawner->NextRequestIndex, 0);
	TestTrue(TEXT("Pending request counts as remaining work"), Spawner->HasAliveEnemies());
	AddExpectedError(TEXT("paused at request"), EAutomationExpectedErrorFlags::Contains, 1);
	Spawner->ProcessNextSpawnRequest();
	TestEqual(TEXT("Retry exhaustion does not clear"), ClearCount, 0);
	TestTrue(TEXT("Retry exhaustion retains request"), Spawner->HasAliveEnemies());
	TestFalse(TEXT("Retry timer stopped"), World->GetTimerManager().IsTimerActive(Spawner->SpawnTimerHandle));
	Spawner->StopSpawning();
	TestFalse(TEXT("Stop cancels pending work"), Spawner->HasAliveEnemies());

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
