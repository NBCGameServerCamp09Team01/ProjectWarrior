#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStatTypes.h"
#include "WarriorStageStatsSubsystem.generated.h"

class AWarriorStageGameState;
class UGameplayAbility;
class UPlayerInventoryComponent;

/** 공격 한 번(어빌리티 발동 1회)을 구분하는 키 */
struct FWarriorAttackKey
{
	FObjectKey Ability;
	uint64 Serial = 0;

	bool operator==(const FWarriorAttackKey& Other) const { return Ability == Other.Ability && Serial == Other.Serial; }
	friend uint32 GetTypeHash(const FWarriorAttackKey& Key) { return HashCombine(GetTypeHash(Key.Ability), GetTypeHash(Key.Serial)); }
};

/** 공격 한 번의 결과 */
struct FWarriorAttackTrack
{
	bool bLanded = false;
	int32 Kills = 0;
};

/** 대상이 마지막으로 맞은 공격 (막타 판정용) */
struct FWarriorLastHit
{
	FWarriorAttackKey AttackKey;
	FName AbilityName = NAME_None;
	bool bByPlayer = false;
};

/**
 * 이번 스테이지 통계 기록기 (S1).
 * 게임 월드마다 생기지만, GameMode가 AWarriorStageGameMode인 월드에서만 기록한다.
 *
 * - 시작: OnWorldBeginPlay에서 GameState를 구독하고 스테이지 기록을 연다.
 * - 진행: GameState의 상태·웨이브 변화로 플레이 시간, 휴식 시간, 웨이브 기록을 채운다.
 *         나머지 값(처치, 데미지 등)은 UWarriorStatsLibrary가 GetMutable* 함수로 채운다 (S1-4).
 * - 종료: StageCleared / StageFailed에서 기록을 마감해 UWarriorProfileStatsSubsystem에 넘긴다.
 *         그 전에 레벨을 떠나면 Deinitialize에서 Abandoned로 마감한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageStatsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStageStatsSubsystem* Get(const UObject* WorldContextObject);

	//~ UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem

	//~ 조회
	/** 스테이지 기록 중이면 true. 스테이지가 아닌 레벨, 클라이언트, 종료 후에는 false */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	bool IsRecording() const { return bRecording && !bRecordSubmitted; }

	/** 진행 중인 스테이지 기록 (이후 HUD용) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const FWarriorStageRecord& GetCurrentStageRecord() const { return Record; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	const FWarriorStatBlock& GetCurrentStageStats() const { return Record.Stats; }

	/** 스테이지 시작 기준 경과 시간 (일시정지 중에는 멈춤) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	float GetStageTimeSeconds() const;

	/** 현재 상태에 머문 시간까지 포함한 실시간 플레이 시간. 기록의 PlayTimeSeconds는 상태 전환 때만 갱신된다 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	float GetLivePlayTimeSeconds() const;

	/** 감시 중인 플레이어 인벤토리의 현재 골드. 감시 전이면 -1 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	int32 GetCurrentGold() const;

	/** 진행 중인 웨이브 번호. 웨이브 전이면 0 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	int32 GetCurrentWaveNumber() const;

	//~ 기록 (UWarriorStatsLibrary 전용). 기록 중이 아니면 nullptr
	FWarriorStageRecord* GetMutableRecord();
	FWarriorStatBlock* GetMutableStats();
	/** 진행 중인 웨이브. 웨이브 전이거나 이미 클리어된 웨이브면 nullptr */
	FWarriorWaveRecord* GetMutableCurrentWave();
	/** 이번 웨이브에 플레이어가 맞았음을 표시 (무피격 웨이브 판정용) */
	void MarkPlayerHitThisWave() { bPlayerHitThisWave = true; }

	//~ 기록 처리 (UWarriorStatsLibrary가 호출). 기록 중이 아니면 아무것도 하지 않는다
	void HandleEnemySpawned(AActor* InEnemy);
	void HandleEnemyKilled(AActor* InEnemy, FName InDeathType);
	void HandleDamage(const FGameplayEffectContextHandle& InContext, AActor* InTarget, float InDamage, float InOverkill, bool bInFatal);
	void HandleBalanceDamage(const FGameplayEffectContextHandle& InContext, AActor* InTarget, float InAmount);
	void HandleHeal(const FGameplayEffectContextHandle& InContext, AActor* InTarget, float InHealed, float InOverheal);
	void HandleAttackAttempt(const UGameplayAbility* InAbility);
	void HandleGoldEarned(int32 InAmount, FName InSource);
	void HandleGoldSpent(int32 InAmount);
	void HandlePotionUsed(FName InItemId);
	void HandlePurchase(FName InItemId, int32 InCount, int32 InGoldSpent);
	void HandleStat(const FGameplayTag& InStatTag, double InValue, FName InDimensionKey);

	/** 플레이어가 조종하는 폰이면 true */
	static bool IsPlayerActor(const AActor* InActor);
	/** 클래스 이름에서 BP 접미사(_C)를 뺀 이름. 적 종류·어빌리티 이름에 사용 */
	static FName GetTypeName(const UObject* InObject);

private:
	void BeginStageRecord(AWarriorStageGameState* InStageGameState);
	void FinishStageRecord(EWarriorStatOutcome InOutcome);

	/** 이전 상태에서 보낸 시간을 플레이 시간·휴식 시간에 더한다 */
	void AccumulateStateTime();
	void OpenWave(int32 InWaveNumber, bool bInBossWave);
	void CloseCurrentWave(bool bInCleared);

	UPlayerInventoryComponent* FindPlayerInventory() const;

	/** 데미지를 준 공격을 구분한다. 공격 시도 기록이 없으면 같은 프레임의 타격을 한 공격으로 본다 */
	FWarriorAttackKey MakeAttackKey(const FGameplayEffectContextHandle& InContext) const;
	void ResetTracking();

	/** 적의 사망 태그가 붙는 순간 사망 방식을 기억한다. 사망 신호 시점에는 태그가 이미 지워졌을 수 있기 때문 */
	void WatchDeathTags(AActor* InEnemy);
	void HandleEnemyDeathTagChanged(const FGameplayTag InTag, int32 InNewCount, TWeakObjectPtr<AActor> InEnemy);
	/** 우선순위: Finisher > Knockback > Normal. 둘 다 None이면 None */
	static FName PickDeathType(FName InA, FName InB);

	UFUNCTION()
	void HandleStageStateChanged(EWarriorStageState NewState, EWarriorStageState OldState);

	UFUNCTION()
	void HandleWaveChanged(int32 WaveNumber, int32 TotalWaveCount, bool bBossWave);

	/** 플레이어 인벤토리 골드 변화를 구독한다. PlayerState가 아직 없으면 false (상태가 바뀔 때마다 다시 시도) */
	bool TryWatchPlayerInventory();
	void UnwatchPlayerInventory();

	/** 골드가 줄어든 만큼을 골드 사용량으로 기록한다. 늘어난 쪽은 발생원(스포너 등)이 따로 기록하므로 무시 */
	UFUNCTION()
	void HandlePlayerGoldChanged(int32 NewGold);

	static bool IsPlayState(EWarriorStageState InState);

	UPROPERTY(Transient)
	FWarriorStageRecord Record;

	TWeakObjectPtr<AWarriorStageGameState> StageGameState;
	TWeakObjectPtr<UPlayerInventoryComponent> WatchedInventory;
	int32 LastKnownGold = 0;

	EWarriorStageState CurrentState = EWarriorStageState::None;
	double StageStartWorldTime = 0.0;
	double StateEnterWorldTime = 0.0;
	/** Record.Waves 안에서 진행 중인 웨이브 위치. 없으면 INDEX_NONE */
	int32 CurrentWaveIndex = INDEX_NONE;
	bool bPlayerHitThisWave = false;

	//~ 추적 (스테이지 동안만)
	TMap<FObjectKey, uint64> AttackSerials;
	TMap<FWarriorAttackKey, FWarriorAttackTrack> AttackTracks;
	TMap<TWeakObjectPtr<AActor>, FWarriorLastHit> LastHits;
	TMap<TWeakObjectPtr<AActor>, float> EnemySpawnTimes;
	TMap<TWeakObjectPtr<AActor>, FName> EnemyDeathTypes;
	bool bRecording = false;
	bool bRecordSubmitted = false;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FWarriorStatsStageTest;
#endif
};
