// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPlayCheckpoint.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "JsonObjectConverter.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/Components/Upgrade/StageUpgradeComponent.h"
#include "ProjectWarrior/DataAssets/DataAsset_Item.h"
#include "ProjectWarrior/DataAssets/DataAsset_Upgrade.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "ProjectWarrior/Stats/WarriorStageStatsSubsystem.h"

namespace WarriorPlayCheckpoint_Private
{
	//스테이지 레벨의 첫 번째 플레이어(싱글 게임)
	struct FPlayerParts
	{
		UPlayerInventoryComponent* Inventory = nullptr;
		UStageUpgradeComponent* Upgrades = nullptr;
		UWarriorAbilitySystemComponent* ASC = nullptr;
		UWarriorStageStatsSubsystem* Stats = nullptr;
	};

	bool FindPlayerParts(const UObject* WorldContextObject, FPlayerParts& OutParts, FString& OutError)
	{
		UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		const AWarriorPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AWarriorPlayerState>() : nullptr;
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!PlayerState || !Pawn)
		{
			OutError = TEXT("no player state or pawn");
			return false;
		}

		OutParts.Inventory = PlayerState->GetPlayerInventoryComponent();
		OutParts.Upgrades = PlayerState->GetStageUpgradeComponent();
		OutParts.ASC = UWarriorFunctionLibrary::NativeGetWarriorASCFromActor(Pawn);
		OutParts.Stats = UWarriorStageStatsSubsystem::Get(World);
		if (!OutParts.Inventory || !OutParts.Upgrades || !OutParts.ASC || !OutParts.Stats)
		{
			OutError = FString::Printf(TEXT("missing part (inventory=%d upgrades=%d asc=%d stats=%d)"),
				OutParts.Inventory != nullptr, OutParts.Upgrades != nullptr, OutParts.ASC != nullptr, OutParts.Stats != nullptr);
			return false;
		}
		return true;
	}

	//클리어한 웨이브 중 가장 큰 번호. 없으면 0
	int32 FindLastClearedWave(const FWarriorStageRecord& InRecord)
	{
		int32 LastCleared = 0;
		for (const FWarriorWaveRecord& Wave : InRecord.Waves)
		{
			if (Wave.bCleared)
			{
				LastCleared = FMath::Max(LastCleared, Wave.WaveNumber);
			}
		}
		return LastCleared;
	}

	//현재값을 [Min, 최대치] 안으로 넣는다
	void SetCurrentAttribute(UWarriorAbilitySystemComponent* InASC, const FGameplayAttribute& InCurrent, const FGameplayAttribute& InMax, float InValue, float InMin)
	{
		const float Max = InASC->GetNumericAttribute(InMax);
		InASC->SetNumericAttributeBase(InCurrent, FMath::Clamp(InValue, FMath::Min(InMin, Max), Max));
	}
}

namespace WarriorPlayCheckpoint
{
	bool Capture(const UObject* WorldContextObject, FWarriorPlayCheckpoint& OutCheckpoint, FString& OutError)
	{
		using namespace WarriorPlayCheckpoint_Private;

		FPlayerParts Parts;
		if (!FindPlayerParts(WorldContextObject, Parts, OutError))
		{
			return false;
		}

		FWarriorPlayCheckpoint Checkpoint;

		//판 통계·시간
		if (!Parts.Stats->CaptureForCheckpoint(Checkpoint.StageRecord, Checkpoint.StageTimeSeconds))
		{
			OutError = TEXT("stage record is not recording");
			return false;
		}
		Checkpoint.ElapsedSeconds = Checkpoint.StageRecord.Stats.PlayTimeSeconds;
		Checkpoint.ClearedWave = FindLastClearedWave(Checkpoint.StageRecord);

		//인벤토리
		Checkpoint.Gold = Parts.Inventory->GetGold();
		for (const FWarriorInventorySlot& Slot : Parts.Inventory->GetSlots())
		{
			if (Slot.IsEmpty())
			{
				continue;
			}
			FWarriorCheckpointItem& Item = Checkpoint.Items.AddDefaulted_GetRef();
			Item.ItemPath = FSoftObjectPath(Slot.ItemData).ToString();
			Item.Quantity = Slot.Quantity;
		}

		//판 강화
		for (const TPair<TObjectPtr<const UDataAsset_Upgrade>, int32>& Pair : Parts.Upgrades->GetUpgradeLevels())
		{
			FWarriorCheckpointUpgrade& Upgrade = Checkpoint.Upgrades.AddDefaulted_GetRef();
			Upgrade.UpgradeId = Pair.Key->UpgradeId;
			Upgrade.UpgradePath = FSoftObjectPath(Pair.Key.Get()).ToString();
			Upgrade.Level = Pair.Value;
		}

		//GAS 현재값
		Checkpoint.Attributes.CurrentHealth = Parts.ASC->GetNumericAttribute(UWarriorAttributeSet::GetCurrentHealthAttribute());
		Checkpoint.Attributes.CurrentStamina = Parts.ASC->GetNumericAttribute(UWarriorAttributeSet::GetCurrentStaminaAttribute());
		Checkpoint.Attributes.CurrentBalance = Parts.ASC->GetNumericAttribute(UWarriorAttributeSet::GetCurrentBalanceAttribute());

		OutCheckpoint = MoveTemp(Checkpoint);
		return true;
	}

	bool Restore(const UObject* WorldContextObject, const FWarriorPlayCheckpoint& InCheckpoint, FString& OutError)
	{
		using namespace WarriorPlayCheckpoint_Private;

		if (InCheckpoint.ContentVersion != FWarriorPlayCheckpoint::CurrentContentVersion)
		{
			OutError = FString::Printf(TEXT("content version %d is not %d"), InCheckpoint.ContentVersion, FWarriorPlayCheckpoint::CurrentContentVersion);
			return false;
		}

		FPlayerParts Parts;
		if (!FindPlayerParts(WorldContextObject, Parts, OutError))
		{
			return false;
		}

		//1) 판 강화: 최대치를 먼저 맞춘다
		TMap<TObjectPtr<const UDataAsset_Upgrade>, int32> UpgradeLevels;
		for (const FWarriorCheckpointUpgrade& Saved : InCheckpoint.Upgrades)
		{
			const UDataAsset_Upgrade* Upgrade = Cast<UDataAsset_Upgrade>(FSoftObjectPath(Saved.UpgradePath).TryLoad());
			if (!Upgrade || Upgrade->UpgradeId != Saved.UpgradeId)
			{
				UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Upgrade skipped. id=%s path=%s"), *Saved.UpgradeId.ToString(), *Saved.UpgradePath);
				continue;
			}
			UpgradeLevels.Add(Upgrade, Saved.Level);
		}
		Parts.Upgrades->RestoreLevels(UpgradeLevels);

		//2) GAS 현재값: 강화로 맞춰진 최대치 안으로. 체크포인트는 웨이브 클리어 뒤라 체력은 1 이상으로 둔다
		const FWarriorCheckpointAttributes& Attributes = InCheckpoint.Attributes;
		SetCurrentAttribute(Parts.ASC, UWarriorAttributeSet::GetCurrentHealthAttribute(), UWarriorAttributeSet::GetMaxHealthAttribute(), Attributes.CurrentHealth, 1.f);
		SetCurrentAttribute(Parts.ASC, UWarriorAttributeSet::GetCurrentStaminaAttribute(), UWarriorAttributeSet::GetMaxStaminaAttribute(), Attributes.CurrentStamina, 0.f);
		SetCurrentAttribute(Parts.ASC, UWarriorAttributeSet::GetCurrentBalanceAttribute(), UWarriorAttributeSet::GetMaxBalanceAttribute(), Attributes.CurrentBalance, 0.f);

		//3) 인벤토리
		TArray<FWarriorInventorySlot> Slots;
		for (const FWarriorCheckpointItem& Saved : InCheckpoint.Items)
		{
			UDataAsset_Item* Item = Cast<UDataAsset_Item>(FSoftObjectPath(Saved.ItemPath).TryLoad());
			if (!Item || Saved.Quantity <= 0)
			{
				UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Item skipped. path=%s quantity=%d"), *Saved.ItemPath, Saved.Quantity);
				continue;
			}
			FWarriorInventorySlot& Slot = Slots.AddDefaulted_GetRef();
			Slot.ItemData = Item;
			Slot.Quantity = Saved.Quantity;
		}
		Parts.Inventory->RestoreState(InCheckpoint.Gold, Slots);

		//4) 판 통계: 마지막(골드 변화가 사용량으로 세지지 않게)
		if (!Parts.Stats->RestoreFromCheckpoint(InCheckpoint.StageRecord, InCheckpoint.StageTimeSeconds))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Stage record not restored (not recording)."));
		}

		UE_LOG(LogProjectWarrior, Log, TEXT("[PlayCheckpoint] Restored. %s"), *Describe(InCheckpoint));
		return true;
	}

	bool ToJson(const FWarriorPlayCheckpoint& InCheckpoint, FString& OutJson)
	{
		return FJsonObjectConverter::UStructToJsonObjectString(InCheckpoint, OutJson, 0, 0, 0, nullptr, false);
	}

	bool FromJson(const FString& InJson, FWarriorPlayCheckpoint& OutCheckpoint)
	{
		return FJsonObjectConverter::JsonObjectStringToUStruct(InJson, &OutCheckpoint);
	}

	FString Describe(const FWarriorPlayCheckpoint& InCheckpoint)
	{
		return FString::Printf(TEXT("version=%d clearedWave=%d elapsed=%.1f s stageTime=%.1f s gold=%d items=%d upgrades=%d hp=%.0f stamina=%.0f balance=%.0f statWaves=%d"),
			InCheckpoint.ContentVersion, InCheckpoint.ClearedWave, InCheckpoint.ElapsedSeconds, InCheckpoint.StageTimeSeconds,
			InCheckpoint.Gold, InCheckpoint.Items.Num(), InCheckpoint.Upgrades.Num(),
			InCheckpoint.Attributes.CurrentHealth, InCheckpoint.Attributes.CurrentStamina, InCheckpoint.Attributes.CurrentBalance,
			InCheckpoint.StageRecord.Waves.Num());
	}
}
