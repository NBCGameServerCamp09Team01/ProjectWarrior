#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WarriorStageWaveTypes.h"
#include "WarriorWaveSpawner.generated.h"

class AWarriorAICharacter;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnWarriorSpawnerEnemyCountChanged,
	int32 /* 현재 생존 수 */,
	int32 /* 생성에 성공한 누적 수 */);
DECLARE_MULTICAST_DELEGATE(FOnWarriorSpawnerWaveCleared);

UCLASS()
class PROJECTWARRIOR_API AWarriorWaveSpawner : public AActor
{
	GENERATED_BODY()

public:
	AWarriorWaveSpawner();

	/** 웨이브 시작을 수락하면 true. 중복 실행이나 잘못된 데이터로 거부하면 false이며 클리어 알림도 발생하지 않는다. */
	bool StartWaveFromData(const FWarriorStageWaveData& InWaveData);
	void StopSpawning();

	int32 GetAliveEnemyCount() const;
	bool HasAliveEnemies() const;

	FOnWarriorSpawnerEnemyCountChanged OnEnemyCountChanged;
	FOnWarriorSpawnerWaveCleared OnWaveCleared;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage")
	TArray<FWarriorWaveSpawnPointData> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.01"))
	float SpawnInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.01"))
	float SpawnRetryInterval = 1.0f;

	/** 요청당 최대 실패 횟수. 한도에 도달하면 웨이브를 클리어하지 않고 스폰을 일시 중단한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (ClampMin = "1"))
	int32 MaxSpawnAttempts = 10;

private:
	bool BuildPendingRequests(const FWarriorStageWaveData& InWaveData);
	void ProcessNextSpawnRequest();
	bool TrySpawnEnemy(const FWarriorPendingWaveSpawnRequest& SpawnRequest);
	bool FindSafeSpawnTransform(const FWarriorWaveSpawnPointData& SpawnPoint, TSubclassOf<AWarriorAICharacter> EnemyClass, FTransform& OutTransform) const;
	int32 SelectWeightedSpawnPoint(const TArray<int32>& Candidates) const;
	void NotifyEnemyCountChanged();
	void TryReportWaveCleared();
	void CompactAliveEnemies();
	void UnbindTrackedEnemies();

	UFUNCTION()
	void HandleSpawnedEnemyDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TArray<FWarriorPendingWaveSpawnRequest> PendingRequests;

	TSet<TWeakObjectPtr<AWarriorAICharacter>> AliveEnemies;
	int32 NextRequestIndex = 0;
	int32 SpawnedEnemyCount = 0;
	int32 CurrentSpawnAttempts = 0;
	uint64 WaveGeneration = 0;
	bool bWaveActive = false;
	bool bWaveClearReported = false;
	FTimerHandle SpawnTimerHandle;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FWarriorWaveSpawnerTest;
#endif
};
