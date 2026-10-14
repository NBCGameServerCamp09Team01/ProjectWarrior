// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorWavePlan.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Stage/WarriorMonsterCatalogDataAsset.h"

namespace WarriorWavePlan
{
	namespace
	{
		//null이거나 없으면 빈 문자열
		FString GetStringOrEmpty(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
		{
			FString Value;
			return InObject->TryGetStringField(InField, Value) ? Value : FString();
		}

		template <typename T>
		T GetNumberOr(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField, T InDefault)
		{
			T Value = InDefault;
			return InObject->TryGetNumberField(InField, Value) ? Value : InDefault;
		}

		bool GetBoolOr(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField, bool bInDefault)
		{
			bool bValue = bInDefault;
			return InObject->TryGetBoolField(InField, bValue) ? bValue : bInDefault;
		}

		FName ToNameOrNone(const FString& InValue)
		{
			return InValue.IsEmpty() ? NAME_None : FName(*InValue);
		}
	}

	void ReadFromResponse(const FString& InBody, FWarriorStageWavePlan& OutPlan)
	{
		OutPlan = FWarriorStageWavePlan();

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InBody);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>* Data = nullptr;
		if (Root->TryGetObjectField(TEXT("data"), Data) && Data)
		{
			ReadFromData(*Data, OutPlan);
		}
	}

	void ReadFromData(const TSharedPtr<FJsonObject>& InData, FWarriorStageWavePlan& OutPlan)
	{
		OutPlan = FWarriorStageWavePlan();
		if (!InData.IsValid())
		{
			return;
		}

		OutPlan.WaveVersion = GetStringOrEmpty(InData, TEXT("waveVersion"));

		const TArray<TSharedPtr<FJsonValue>>* WaveValues = nullptr;
		if (!InData->TryGetArrayField(TEXT("waves"), WaveValues))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& WaveValue : *WaveValues)
		{
			const TSharedPtr<FJsonObject> WaveObject = WaveValue.IsValid() ? WaveValue->AsObject() : nullptr;
			if (!WaveObject.IsValid())
			{
				continue;
			}

			FWarriorWavePlanWave& Wave = OutPlan.Waves.AddDefaulted_GetRef();
			Wave.WaveNumber = GetNumberOr<int32>(WaveObject, TEXT("waveNumber"), OutPlan.Waves.Num());
			Wave.RestSeconds = GetNumberOr<float>(WaveObject, TEXT("restSeconds"), 0.f);
			Wave.bBossWave = GetBoolOr(WaveObject, TEXT("bossWave"), false);
			Wave.GoldChanceMultiplier = GetNumberOr<float>(WaveObject, TEXT("goldChanceMultiplier"), 1.f);
			Wave.GoldAmountMultiplier = GetNumberOr<float>(WaveObject, TEXT("goldAmountMultiplier"), 1.f);

			const TArray<TSharedPtr<FJsonValue>>* EnemyValues = nullptr;
			if (!WaveObject->TryGetArrayField(TEXT("enemies"), EnemyValues))
			{
				continue;
			}

			for (const TSharedPtr<FJsonValue>& EnemyValue : *EnemyValues)
			{
				const TSharedPtr<FJsonObject> EnemyObject = EnemyValue.IsValid() ? EnemyValue->AsObject() : nullptr;
				if (!EnemyObject.IsValid())
				{
					continue;
				}

				FWarriorWavePlanEnemy& Enemy = Wave.Enemies.AddDefaulted_GetRef();
				Enemy.MonsterId = ToNameOrNone(GetStringOrEmpty(EnemyObject, TEXT("monsterId")));
				Enemy.Count = GetNumberOr<int32>(EnemyObject, TEXT("count"), 0);
				Enemy.bBoss = GetBoolOr(EnemyObject, TEXT("boss"), false);
				Enemy.SpawnGroup = ToNameOrNone(GetStringOrEmpty(EnemyObject, TEXT("spawnGroup")));
				Enemy.GoldReward = GetNumberOr<int32>(EnemyObject, TEXT("goldReward"), 0);
				Enemy.GoldDropChance = GetNumberOr<float>(EnemyObject, TEXT("goldDropChance"), 1.f);
			}
		}
	}

	bool FindUnknownMonster(const FWarriorStageWavePlan& InPlan, const UWarriorMonsterCatalogDataAsset* InCatalog, FName& OutUnknown)
	{
		OutUnknown = NAME_None;
		for (const FWarriorWavePlanWave& Wave : InPlan.Waves)
		{
			for (const FWarriorWavePlanEnemy& Enemy : Wave.Enemies)
			{
				if (!InCatalog || !InCatalog->HasMonster(Enemy.MonsterId))
				{
					OutUnknown = Enemy.MonsterId;
					return true;
				}
			}
		}
		return false;
	}

	bool ToWaveData(const FWarriorStageWavePlan& InPlan, const UWarriorMonsterCatalogDataAsset* InCatalog,
		TArray<FWarriorStageWaveData>& OutWaves, FName& OutUnknown)
	{
		OutWaves.Reset();
		OutUnknown = NAME_None;
		if (!InPlan.IsSet() || FindUnknownMonster(InPlan, InCatalog, OutUnknown))
		{
			return false;
		}

		for (const FWarriorWavePlanWave& PlanWave : InPlan.Waves)
		{
			FWarriorStageWaveData& Wave = OutWaves.AddDefaulted_GetRef();
			Wave.RestTimeOverride = PlanWave.RestSeconds;
			Wave.bBossWave = PlanWave.bBossWave;
			Wave.DropModifier.GoldDropChanceMultiplier = PlanWave.GoldChanceMultiplier;
			Wave.DropModifier.GoldAmountMultiplier = PlanWave.GoldAmountMultiplier;

			for (const FWarriorWavePlanEnemy& PlanEnemy : PlanWave.Enemies)
			{
				//카탈로그에 있는 것은 위에서 확인했다. BP를 읽지 못하면(경로가 깨짐) 모르는 몬스터와 같게 본다
				UClass* EnemyClass = InCatalog->FindMonster(PlanEnemy.MonsterId).LoadSynchronous();
				if (!EnemyClass)
				{
					OutWaves.Reset();
					OutUnknown = PlanEnemy.MonsterId;
					return false;
				}

				FWarriorWaveEnemySpawnData& Enemy = Wave.Enemies.AddDefaulted_GetRef();
				Enemy.EnemyClass = EnemyClass;
				Enemy.Count = PlanEnemy.Count;
				Enemy.bBossEnemy = PlanEnemy.bBoss;
				Enemy.SpawnGroup = PlanEnemy.SpawnGroup;
				Enemy.GoldReward = PlanEnemy.GoldReward;
				Enemy.GoldDropChance = PlanEnemy.GoldDropChance;
			}
		}
		return true;
	}
}
