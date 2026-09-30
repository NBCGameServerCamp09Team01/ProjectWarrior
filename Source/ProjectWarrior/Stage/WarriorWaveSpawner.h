#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WarriorStageWaveTypes.h"
#include "WarriorWaveSpawner.generated.h"

class AWarriorAICharacter;
class AWarriorBaseCharacter;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnWarriorSpawnerEnemyCountChanged,
	int32 /* 현재 생존 수 */,
	int32 /* 생성에 성공한 누적 수 */);
DECLARE_MULTICAST_DELEGATE(FOnWarriorSpawnerWaveCleared);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnWarriorSpawnerEnemyRewarded,
	AWarriorAICharacter* /* 처치된 적 */,
	int32 /* 지급한 골드 */);

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
	/** 아직 생성하거나 건너뛰지 않은 요청 수. */
	int32 GetRemainingSpawnCount() const { return FMath::Max(0, PendingRequests.Num() - NextRequestIndex); }
	bool HasAliveEnemies() const;

	FOnWarriorSpawnerEnemyCountChanged OnEnemyCountChanged;
	FOnWarriorSpawnerWaveCleared OnWaveCleared;
	/** 사망 신호로 처치된 적의 골드 보상이 확정되면 방송한다. Destroy로만 사라진 적은 보상 없음. */
	FOnWarriorSpawnerEnemyRewarded OnEnemyRewarded;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage")
	TArray<FWarriorWaveSpawnPointData> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.01"))
	float SpawnInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.01"))
	float SpawnRetryInterval = 1.0f;

	/** 요청당 최대 실패 횟수. 한도에 도달하면 오류 로그를 남기고 해당 요청을 건너뛴다. */
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
	void UnbindEnemy(AWarriorAICharacter* Enemy);
	/** 생존 목록에서 적을 빼고 수량 알림·클리어 판정을 한다. 이미 빠진 적이면 false. */
	bool RemoveTrackedEnemy(AWarriorAICharacter* Enemy, const TCHAR* Reason);
	/** 확률·배율을 적용해 골드를 계산하고, 지급 대상이 있으면 플레이어 인벤토리에 넣는다. 지급액(0이면 미지급)을 반환. */
	int32 GrantEnemyReward(AWarriorAICharacter* Enemy);
	void GiveGoldToPlayer(int32 InGold) const;

	/** 사망 연출이 끝나 OnCharacterDied가 방송되면 호출된다. 시체가 남아도 생존 수에서 뺀다. */
	UFUNCTION()
	void HandleSpawnedEnemyDied(AWarriorBaseCharacter* DeadCharacter);

	/** 사망 신호 없이 Destroy된 경우(콘솔 삭제, 레벨 정리 등)의 예비 경로. */
	UFUNCTION()
	void HandleSpawnedEnemyDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TArray<FWarriorPendingWaveSpawnRequest> PendingRequests;

	TSet<TWeakObjectPtr<AWarriorAICharacter>> AliveEnemies;
	TMap<TWeakObjectPtr<AWarriorAICharacter>, FWarriorWaveEnemyReward> EnemyRewards;
	/** 진행 중 웨이브의 골드 배율. 웨이브 시작 시 복사한다. */
	FWarriorDropModifier CurrentDropModifier;
	int32 NextRequestIndex = 0;
	int32 SpawnedEnemyCount = 0;
	int32 CurrentSpawnAttempts = 0;
	uint64 WaveGeneration = 0;
	bool bWaveActive = false;
	bool bWaveClearReported = false;
	FTimerHandle SpawnTimerHandle;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FWarriorWaveSpawnerTest;
	friend class FWarriorWaveSpawnerDeathRewardTest;
#endif
};
