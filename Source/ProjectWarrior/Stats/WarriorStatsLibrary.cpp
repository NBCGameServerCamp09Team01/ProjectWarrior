#include "WarriorStatsLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "WarriorProfileStatsSubsystem.h"
#include "WarriorStageStatsSubsystem.h"

namespace WarriorStatsLibrary_Private
{
	/** 기록 중인 스테이지 기록기. 없거나 기록 중이 아니면 nullptr */
	UWarriorStageStatsSubsystem* GetRecorder(const UObject* WorldContextObject)
	{
		UWarriorStageStatsSubsystem* StageStats = UWarriorStageStatsSubsystem::Get(WorldContextObject);
		return StageStats && StageStats->IsRecording() ? StageStats : nullptr;
	}

	const UObject* GetContextWorldObject(const FGameplayEffectContextHandle& EffectContext, const AActor* Target)
	{
		return Target ? static_cast<const UObject*>(Target) : EffectContext.GetInstigator();
	}
}

void UWarriorStatsLibrary::RecordEnemySpawned(AActor* Enemy)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(Enemy))
	{
		Recorder->HandleEnemySpawned(Enemy);
	}
}

void UWarriorStatsLibrary::RecordEnemyKilled(AActor* Enemy, const FName DeathType)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(Enemy))
	{
		Recorder->HandleEnemyKilled(Enemy, DeathType);
	}
}

void UWarriorStatsLibrary::RecordDamage(const FGameplayEffectContextHandle& EffectContext, AActor* Target, const float Damage, const float Overkill, const bool bFatal)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WarriorStatsLibrary_Private::GetContextWorldObject(EffectContext, Target)))
	{
		Recorder->HandleDamage(EffectContext, Target, Damage, Overkill, bFatal);
	}
}

void UWarriorStatsLibrary::RecordBalanceDamage(const FGameplayEffectContextHandle& EffectContext, AActor* Target, const float Amount)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WarriorStatsLibrary_Private::GetContextWorldObject(EffectContext, Target)))
	{
		Recorder->HandleBalanceDamage(EffectContext, Target, Amount);
	}
}

void UWarriorStatsLibrary::RecordHeal(const FGameplayEffectContextHandle& EffectContext, AActor* Target, const float Healed, const float Overheal)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WarriorStatsLibrary_Private::GetContextWorldObject(EffectContext, Target)))
	{
		Recorder->HandleHeal(EffectContext, Target, Healed, Overheal);
	}
}

void UWarriorStatsLibrary::RecordAttackAttempt(const UGameplayAbility* Ability)
{
	const AActor* Avatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(Avatar))
	{
		Recorder->HandleAttackAttempt(Ability);
	}
}

void UWarriorStatsLibrary::RecordGoldEarned(const UObject* WorldContextObject, const int32 Amount, const FName Source)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WorldContextObject))
	{
		Recorder->HandleGoldEarned(Amount, Source);
	}
}

void UWarriorStatsLibrary::RecordGoldSpent(const UObject* WorldContextObject, const int32 Amount)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WorldContextObject))
	{
		Recorder->HandleGoldSpent(Amount);
	}
}

void UWarriorStatsLibrary::RecordPotionUsed(const UObject* WorldContextObject, const FName ItemId)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WorldContextObject))
	{
		Recorder->HandlePotionUsed(ItemId);
	}
}

void UWarriorStatsLibrary::RecordPurchase(const UObject* WorldContextObject, const FName ItemId, const int32 Count, const int32 GoldSpent)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WorldContextObject))
	{
		Recorder->HandlePurchase(ItemId, Count, GoldSpent);
	}
}

void UWarriorStatsLibrary::RecordStat(const UObject* WorldContextObject, const FGameplayTag StatTag, const double Value, const FName DimensionKey)
{
	if (UWarriorStageStatsSubsystem* Recorder = WarriorStatsLibrary_Private::GetRecorder(WorldContextObject))
	{
		Recorder->HandleStat(StatTag, Value, DimensionKey);
	}
}

UWarriorStageStatsSubsystem* UWarriorStatsLibrary::GetStageStats(const UObject* WorldContextObject)
{
	return UWarriorStageStatsSubsystem::Get(WorldContextObject);
}

UWarriorProfileStatsSubsystem* UWarriorStatsLibrary::GetProfileStats(const UObject* WorldContextObject)
{
	return UWarriorProfileStatsSubsystem::Get(WorldContextObject);
}

bool UWarriorStatsLibrary::FindExtraStat(const FWarriorStatBlock& Stats, const FGameplayTag StatTag, const FName DimensionKey, FWarriorStatValue& OutValue)
{
	if (const FWarriorStatValue* Found = Stats.FindExtra(StatTag, DimensionKey))
	{
		OutValue = *Found;
		return true;
	}
	OutValue = FWarriorStatValue();
	return false;
}

FName UWarriorStatsLibrary::GetEnemyTypeName(const AActor* Enemy)
{
	return UWarriorStageStatsSubsystem::GetTypeName(Enemy);
}

FName UWarriorStatsLibrary::GetDeathTypeName(AActor* DeadActor)
{
	// ASC가 없는 액터에서도 안전하도록 WarriorFunctionLibrary(CastChecked) 대신 직접 조회한다.
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DeadActor);
	if (!ASC)
	{
		return NAME_None;
	}
	if (ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Death_Finisher)
		|| ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Finisher))
	{
		return TEXT("Finisher");
	}
	if (ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Death_Knockback))
	{
		return TEXT("Knockback");
	}
	if (ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Death_Normal))
	{
		return TEXT("Normal");
	}
	return NAME_None;
}
