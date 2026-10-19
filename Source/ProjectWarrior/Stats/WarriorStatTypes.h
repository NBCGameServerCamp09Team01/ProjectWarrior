#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WarriorStatTypes.generated.h"

/**
 * 게임 통계 데이터 모델 (S1).
 * - 요청 항목은 이름 있는 필드로 둔다. 그 밖의 추가 통계는 FWarriorStatBlock::Extra에 태그로 쌓는다.
 * - 비율·평균 같은 파생 값은 저장하지 않고 계산 함수로 구한다.
 * - 같은 FWarriorStatBlock을 스테이지 / 게임 한 판(Run) / 누적에 똑같이 쓰고, 아래 층을 위 층에 Merge한다.
 */

UENUM(BlueprintType)
enum class EWarriorStatOutcome : uint8
{
	InProgress,
	Cleared,
	Failed,
	Abandoned
};

/** 범용 통계 값. 한 키에 들어온 값의 합계·최댓값·최솟값·횟수를 모두 담고, 태그별 집계 방식에 맞는 필드를 읽는다. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStatValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double Sum = 0.0;

	/** Count가 0이면 의미 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double Max = 0.0;

	/** Count가 0이면 의미 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double Min = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Count = 0;

	void Add(double InValue);
	void Merge(const FWarriorStatValue& Other);
};

/** Extra 통계 한 항목: 태그 + 세부 키(적 종류 등, 없으면 None) → 값 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStatEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FGameplayTag StatTag;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FName DimensionKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorStatValue Value;
};

/** 공격 통계 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAttackStats
{
	GENERATED_BODY()

	/** 공격 시도 횟수 (공격 어빌리티 발동) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 AttackAttempts = 0;

	/** 공격 횟수 (1명 이상 맞힌 시도) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 AttacksLanded = 0;

	/** 적중 타격 수 (맞은 대상 수의 합) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 HitsDealt = 0;

	/** 가한 데미지 합계 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double DamageDealt = 0.0;

	/** 가한 최고 데미지 (한 타) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double MaxDamageDealt = 0.0;

	/** 죽인 적 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Kills = 0;

	/** 한 공격에 죽인 최대 적 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 MaxKillsPerAttack = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, int32> KillsByEnemyType;

	/** Normal / Knockback / Finisher */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, int32> KillsByDeathType;

	/** 무기·어빌리티별 가한 데미지 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, double> DamageByAbility;

	/** 공격 비율 = 공격 횟수 / 공격 시도 횟수. 시도가 0이면 0 */
	double GetHitRate() const;

	/** 평균 데미지 = 가한 데미지 / 적중 타격 수. 타격이 0이면 0 */
	double GetAverageDamage() const;

	void Merge(const FWarriorAttackStats& Other);
};

/** 피격 통계 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorDefenseStats
{
	GENERATED_BODY()

	/** 피격 횟수 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 HitsTaken = 0;

	/** 피격 데미지 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double DamageTaken = 0.0;

	/** 사망 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, double> DamageTakenByEnemyType;

	void Merge(const FWarriorDefenseStats& Other);
};

/** 회복 통계 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorHealStats
{
	GENERATED_BODY()

	/** 회복량 (실제로 오른 체력) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double HealAmount = 0.0;

	/** 최대 체력을 넘어 버려진 회복량 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double Overheal = 0.0;

	/** 마신 포션 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 PotionsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, int32> PotionsUsedByItem;

	void Merge(const FWarriorHealStats& Other);
};

/** 아이템 한 종류의 구매 합계 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorItemPurchaseStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 GoldSpent = 0;
};

/** 경제·상점 통계 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorEconomyStats
{
	GENERATED_BODY()

	/** 골드 획득량 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 GoldEarned = 0;

	/** 골드 사용량 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 GoldSpent = 0;

	/** 획득 경로별 (Kill, Reward ...) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, int32> GoldEarnedBySource;

	/** 아이템 종류별 구매 개수·쓴 골드 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, FWarriorItemPurchaseStats> PurchasesByItem;

	void Merge(const FWarriorEconomyStats& Other);
};

/** 한 층(스테이지 / 판 / 누적)의 통계 묶음 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStatBlock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorAttackStats Attack;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorDefenseStats Defense;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorHealStats Heal;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorEconomyStats Economy;

	/** 실제 진행 시간 (일시정지 제외) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double PlayTimeSeconds = 0.0;

	/** 명시 필드 밖의 추가 통계 (WarriorStatTags). 항목 수가 적어 배열로 둔다 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TArray<FWarriorStatEntry> Extra;

	/** Extra에 값 하나를 더한다. 같은 태그·세부 키가 있으면 그 항목에 누적 */
	void AddExtra(const FGameplayTag& InStatTag, double InValue, FName InDimensionKey = NAME_None);

	/** 없으면 nullptr */
	const FWarriorStatValue* FindExtra(const FGameplayTag& InStatTag, FName InDimensionKey = NAME_None) const;

	/** 아래 층 값을 이 층에 합산한다. 합계는 더하고, 최댓값·최솟값은 비교한다 */
	void Merge(const FWarriorStatBlock& Other);

private:
	FWarriorStatValue& FindOrAddExtra(const FGameplayTag& InStatTag, FName InDimensionKey);
};

/** 아이템 구매 한 건 (시점 포함) */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorPurchaseRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 GoldSpent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 WaveNumber = 0;

	/** 스테이지 시작 기준 경과 시간 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	float TimeSeconds = 0.0f;
};

/** 플레이어 사망 한 건 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorDeathRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FName KillerEnemyType = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FName KillerAbility = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	float TimeSeconds = 0.0f;
};

/** 웨이브 한 개의 가벼운 기록 (웨이브는 집계 단위가 아니다) */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorWaveRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	bool bBossWave = false;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	float StartTimeSeconds = 0.0f;

	/** bCleared가 false면 의미 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	float ClearTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	bool bCleared = false;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Kills = 0;
};

/** 적 종류별 통계 (밸런스용) */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorEnemyTypeStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FName EnemyType = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Spawned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 Killed = 0;

	/** 스폰부터 사망까지 걸린 시간의 합. 평균 = TotalTimeToKill / Killed */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double TotalTimeToKill = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	double DamageToPlayer = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 PlayerKills = 0;

	double GetAverageTimeToKill() const;

	void Merge(const FWarriorEnemyTypeStats& Other);
};

/** 스테이지 한 번의 기록 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 SchemaVersion = 1;

	/** 프로젝트 설정 ProjectVersion. 패치 전후 비교용 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FString BuildVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FGuid RecordId;

	/** 이 스테이지가 속한 판 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FGuid RunId;

	/** 레벨 이름 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FName StageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	EWarriorStatOutcome Outcome = EWarriorStatOutcome::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FDateTime StartedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FDateTime EndedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 WavesCleared = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 TotalWaves = 0;

	/** 종료 시 보유 골드 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 GoldAtEnd = 0;

	/** 스테이지 합계 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorStatBlock Stats;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TArray<FWarriorWaveRecord> Waves;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TArray<FWarriorPurchaseRecord> Purchases;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TArray<FWarriorDeathRecord> Deaths;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TArray<FWarriorEnemyTypeStats> Enemies;

	/** 없으면 추가해서 돌려준다 */
	FWarriorEnemyTypeStats& FindOrAddEnemyType(FName InEnemyType);
};

/** 게임 한 판(Run)의 기록 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorRunRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FString BuildVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	EWarriorStatOutcome Outcome = EWarriorStatOutcome::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FDateTime StartedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FDateTime EndedAtUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 StagesPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 StagesCleared = 0;

	/** 판 합계 (= 스테이지 합) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorStatBlock Stats;

	/** 이 판에서 플레이한 스테이지 기록 ID */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TArray<FGuid> StageRecordIds;

	bool IsActive() const { return RunId.IsValid() && Outcome == EWarriorStatOutcome::InProgress; }
};

/** 누적 통계 (이번 실행 중 여러 판 합산. 디스크 저장은 이후 확장) */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorLifetimeStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	FWarriorStatBlock Stats;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 RunsPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 RunsCleared = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 StagesPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 StagesCleared = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 StagesFailed = 0;

	/** 0이면 기록 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	float BestStageClearTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	int32 MaxWaveReached = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stats")
	TMap<FName, FWarriorEnemyTypeStats> EnemiesByType;
};
