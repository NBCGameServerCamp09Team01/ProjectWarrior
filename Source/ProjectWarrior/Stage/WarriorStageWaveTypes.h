#pragma once

#include "CoreMinimal.h"
#include "WarriorStageWaveTypes.generated.h"

class AWarriorAICharacter;

/** 웨이브에 적용할 골드 보상의 보정 배율을 정의한다. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorDropModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float GoldDropChanceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float GoldAmountMultiplier = 1.0f;
};

/** 웨이브에서 생성할 적 한 종류와 생성 수량을 정의한다. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorWaveEnemySpawnData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	TSubclassOf<AWarriorAICharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	bool bBossEnemy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	FName SpawnGroup = NAME_None;

	/** 처치 시 지급할 기본 골드. 웨이브 DropModifier.GoldAmountMultiplier가 곱해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage|Reward", meta = (ClampMin = "0"))
	int32 GoldReward = 0;

	/** 처치 시 골드 지급 확률(0~1). 웨이브 DropModifier.GoldDropChanceMultiplier가 곱해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage|Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GoldDropChance = 1.0f;
};

/** 스테이지 내 단일 웨이브의 편집용 데이터. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageWaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	TArray<FWarriorWaveEnemySpawnData> Enemies;

	/** 0이면 GameMode의 기본 휴식 시간을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float RestTimeOverride = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	bool bBossWave = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	FWarriorDropModifier DropModifier;
};

/** 지정한 그룹의 스폰 위치로 선택할 레벨 액터와 선택 가중치. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorWaveSpawnPointData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	TObjectPtr<AActor> SpawnPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage")
	FName SpawnGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warrior|Stage", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

/** 적 생성 수량을 개별 요청으로 펼친 큐 항목. AWarriorWaveSpawner가 순서대로 처리한다. */
USTRUCT()
struct PROJECTWARRIOR_API FWarriorPendingWaveSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AWarriorAICharacter> EnemyClass;

	UPROPERTY()
	FName SpawnGroup = NAME_None;

	UPROPERTY()
	int32 GoldReward = 0;

	UPROPERTY()
	float GoldDropChance = 1.0f;
};

/** 스폰된 적 한 마리에 붙는 처치 보상 정보. */
struct FWarriorWaveEnemyReward
{
	int32 GoldReward = 0;
	float GoldDropChance = 1.0f;
};
