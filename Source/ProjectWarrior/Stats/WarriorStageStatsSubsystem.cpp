#include "WarriorStageStatsSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/GameModes/WarriorFrontGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"
#include "ProjectWarrior/GameModes/WarriorStageGameState.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "WarriorProfileStatsSubsystem.h"
#include "WarriorStatTags.h"

UWarriorStageStatsSubsystem* UWarriorStageStatsSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWarriorStageStatsSubsystem>() : nullptr;
}

bool UWarriorStageStatsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 에디터·미리보기 월드에서는 만들지 않는다.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWarriorStageStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 기록은 권한 쪽에서만 한다 (D7). 싱글 플레이에서는 항상 권한 쪽이다.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	AGameModeBase* GameMode = InWorld.GetAuthGameMode();
	if (Cast<AWarriorFrontGameMode>(GameMode))
	{
		// 타이틀로 돌아오면 진행 중인 판은 중도 이탈로 끝낸다 (D9).
		if (UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(&InWorld))
		{
			if (ProfileStats->HasActiveRun())
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Front level entered with an active run. Ending it as abandoned."));
				ProfileStats->EndRun(EWarriorStatOutcome::Abandoned);
			}
		}
		return;
	}

	if (!Cast<AWarriorStageGameMode>(GameMode))
	{
		return;
	}

	AWarriorStageGameState* GameState = InWorld.GetGameState<AWarriorStageGameState>();
	if (!GameState)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] Stage level without AWarriorStageGameState. Stats will not be recorded."));
		return;
	}

	BeginStageRecord(GameState);
}

void UWarriorStageStatsSubsystem::Deinitialize()
{
	// 스테이지가 끝나기 전에 레벨을 떠났으면 중도 이탈로 마감한다.
	if (IsRecording())
	{
		FinishStageRecord(EWarriorStatOutcome::Abandoned);
	}

	if (AWarriorStageGameState* GameState = StageGameState.Get())
	{
		GameState->OnStageStateChanged.RemoveDynamic(this, &ThisClass::HandleStageStateChanged);
		GameState->OnWaveChanged.RemoveDynamic(this, &ThisClass::HandleWaveChanged);
	}
	StageGameState.Reset();

	Super::Deinitialize();
}

float UWarriorStageStatsSubsystem::GetStageTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return bRecording && World ? static_cast<float>(World->GetTimeSeconds() - StageStartWorldTime) : 0.0f;
}

int32 UWarriorStageStatsSubsystem::GetCurrentWaveNumber() const
{
	return Record.Waves.IsValidIndex(CurrentWaveIndex) ? Record.Waves[CurrentWaveIndex].WaveNumber : 0;
}

FWarriorStageRecord* UWarriorStageStatsSubsystem::GetMutableRecord()
{
	return IsRecording() ? &Record : nullptr;
}

FWarriorStatBlock* UWarriorStageStatsSubsystem::GetMutableStats()
{
	return IsRecording() ? &Record.Stats : nullptr;
}

FWarriorWaveRecord* UWarriorStageStatsSubsystem::GetMutableCurrentWave()
{
	if (!IsRecording() || !Record.Waves.IsValidIndex(CurrentWaveIndex))
	{
		return nullptr;
	}
	FWarriorWaveRecord& Wave = Record.Waves[CurrentWaveIndex];
	return Wave.bCleared ? nullptr : &Wave;
}

void UWarriorStageStatsSubsystem::BeginStageRecord(AWarriorStageGameState* InStageGameState)
{
	UWorld* World = GetWorld();
	UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(World);

	Record = FWarriorStageRecord();
	Record.RecordId = FGuid::NewGuid();
	Record.RunId = ProfileStats ? ProfileStats->BeginRunIfNeeded() : FGuid();
	Record.BuildVersion = UWarriorProfileStatsSubsystem::GetBuildVersion();
	Record.StageId = FName(*UWorld::RemovePIEPrefix(World->GetMapName()));
	Record.StartedAtUtc = FDateTime::UtcNow();

	StageStartWorldTime = World->GetTimeSeconds();
	StateEnterWorldTime = StageStartWorldTime;
	CurrentState = InStageGameState->GetStageState();
	CurrentWaveIndex = INDEX_NONE;
	bPlayerHitThisWave = false;
	ResetTracking();
	bRecording = true;
	bRecordSubmitted = false;

	StageGameState = InStageGameState;
	InStageGameState->OnStageStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStageStateChanged);
	InStageGameState->OnWaveChanged.AddUniqueDynamic(this, &ThisClass::HandleWaveChanged);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stats] Stage record started: %s (run %s)"), *Record.StageId.ToString(), *Record.RunId.ToString());
}

void UWarriorStageStatsSubsystem::FinishStageRecord(const EWarriorStatOutcome InOutcome)
{
	if (!IsRecording())
	{
		return;
	}

	AccumulateStateTime();
	if (Record.Waves.IsValidIndex(CurrentWaveIndex) && !Record.Waves[CurrentWaveIndex].bCleared)
	{
		CloseCurrentWave(false);
	}

	Record.Outcome = InOutcome;
	Record.EndedAtUtc = FDateTime::UtcNow();
	if (const AWarriorStageGameState* GameState = StageGameState.Get())
	{
		Record.TotalWaves = GameState->GetTotalWaveCount();
	}
	if (const UPlayerInventoryComponent* Inventory = FindPlayerInventory())
	{
		Record.GoldAtEnd = Inventory->GetGold();
	}

	// 먼저 표시해 두어 넘기는 도중 다시 불려도 두 번 넘기지 않게 한다.
	bRecordSubmitted = true;

	if (UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(GetWorld()))
	{
		ProfileStats->AddStageRecord(Record);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stats] No profile stats subsystem. Stage record %s was not stored."), *Record.StageId.ToString());
	}
}

bool UWarriorStageStatsSubsystem::IsPlayState(const EWarriorStageState InState)
{
	switch (InState)
	{
	case EWarriorStageState::Preparing:
	case EWarriorStageState::InProgress:
	case EWarriorStageState::WaveCleared:
	case EWarriorStageState::Resting:
		return true;
	default:
		return false;
	}
}

void UWarriorStageStatsSubsystem::AccumulateStateTime()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 월드 시간은 일시정지 중에 멈추므로 일시정지 시간은 자동으로 빠진다.
	const double Now = World->GetTimeSeconds();
	const double Elapsed = FMath::Max(0.0, Now - StateEnterWorldTime);
	StateEnterWorldTime = Now;

	if (Elapsed <= 0.0)
	{
		return;
	}
	if (IsPlayState(CurrentState))
	{
		Record.Stats.PlayTimeSeconds += Elapsed;
	}
	if (CurrentState == EWarriorStageState::Resting)
	{
		Record.Stats.AddExtra(WarriorStatTags::Stat_Time_Rest, Elapsed);
	}
}

void UWarriorStageStatsSubsystem::OpenWave(const int32 InWaveNumber, const bool bInBossWave)
{
	if (Record.Waves.IsValidIndex(CurrentWaveIndex) && !Record.Waves[CurrentWaveIndex].bCleared)
	{
		// 클리어 전에 다음 웨이브가 시작되면(시작 실패로 건너뛴 경우 등) 미클리어로 닫는다.
		CloseCurrentWave(false);
	}

	FWarriorWaveRecord& Wave = Record.Waves.AddDefaulted_GetRef();
	Wave.WaveNumber = InWaveNumber;
	Wave.bBossWave = bInBossWave;
	Wave.StartTimeSeconds = GetStageTimeSeconds();
	CurrentWaveIndex = Record.Waves.Num() - 1;
	bPlayerHitThisWave = false;
}

void UWarriorStageStatsSubsystem::CloseCurrentWave(const bool bInCleared)
{
	if (!Record.Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	FWarriorWaveRecord& Wave = Record.Waves[CurrentWaveIndex];
	if (Wave.bCleared)
	{
		return;
	}

	Wave.bCleared = bInCleared;
	if (bInCleared)
	{
		Wave.ClearTimeSeconds = GetStageTimeSeconds();
		++Record.WavesCleared;
		if (!bPlayerHitThisWave)
		{
			Record.Stats.AddExtra(WarriorStatTags::Stat_Defense_Wave_NoHit, 1.0);
		}
	}
}

UPlayerInventoryComponent* UWarriorStageStatsSubsystem::FindPlayerInventory() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const AWarriorPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AWarriorPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetPlayerInventoryComponent() : nullptr;
}

void UWarriorStageStatsSubsystem::HandleStageStateChanged(const EWarriorStageState NewState, const EWarriorStageState OldState)
{
	if (!IsRecording())
	{
		return;
	}

	AccumulateStateTime();
	CurrentState = NewState;

	switch (NewState)
	{
	case EWarriorStageState::WaveCleared:
		CloseCurrentWave(true);
		break;
	case EWarriorStageState::StageCleared:
		// 마지막 웨이브는 WaveCleared를 거치지 않고 바로 StageCleared로 갈 수 있다.
		CloseCurrentWave(true);
		FinishStageRecord(EWarriorStatOutcome::Cleared);
		break;
	case EWarriorStageState::StageFailed:
		FinishStageRecord(EWarriorStatOutcome::Failed);
		break;
	default:
		break;
	}
}

void UWarriorStageStatsSubsystem::HandleWaveChanged(const int32 WaveNumber, const int32 TotalWaveCount, const bool bBossWave)
{
	if (!IsRecording())
	{
		return;
	}

	Record.TotalWaves = TotalWaveCount;

	// GameMode는 등록 직후 0번(웨이브 전)으로 한 번 방송한다. 같은 번호가 다시 와도 새로 열지 않는다.
	if (WaveNumber <= 0 || WaveNumber == GetCurrentWaveNumber())
	{
		return;
	}

	OpenWave(WaveNumber, bBossWave);
}

//~ 기록 처리

bool UWarriorStageStatsSubsystem::IsPlayerActor(const AActor* InActor)
{
	const APawn* Pawn = Cast<APawn>(InActor);
	return Pawn && Pawn->IsPlayerControlled();
}

FName UWarriorStageStatsSubsystem::GetTypeName(const UObject* InObject)
{
	if (!InObject)
	{
		return NAME_None;
	}

	FString ClassName = InObject->GetClass()->GetName();
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FName(*ClassName);
}

void UWarriorStageStatsSubsystem::ResetTracking()
{
	AttackSerials.Reset();
	AttackTracks.Reset();
	LastHits.Reset();
	EnemySpawnTimes.Reset();
	EnemyDeathTypes.Reset();
}

FName UWarriorStageStatsSubsystem::PickDeathType(const FName InA, const FName InB)
{
	static const FName Finisher(TEXT("Finisher"));
	static const FName Knockback(TEXT("Knockback"));
	static const FName Normal(TEXT("Normal"));
	auto Rank = [&](const FName Type) { return Type == Finisher ? 3 : Type == Knockback ? 2 : Type == Normal ? 1 : 0; };
	return Rank(InA) >= Rank(InB) ? InA : InB;
}

void UWarriorStageStatsSubsystem::WatchDeathTags(AActor* InEnemy)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InEnemy);
	if (!ASC)
	{
		return;
	}

	const TWeakObjectPtr<AActor> WeakEnemy(InEnemy);
	for (const FGameplayTag& DeathTag : {
		WarriorGameplayTags::Shared_Status_Death_Normal.GetTag(),
		WarriorGameplayTags::Shared_Status_Death_Knockback.GetTag(),
		WarriorGameplayTags::Shared_Status_Death_Finisher.GetTag(),
		// 플레이어 처형(GA_Player_Finisher)으로 죽는 적은 Death.* 태그 없이 처형 상태 태그만 붙을 수 있다.
		WarriorGameplayTags::Shared_Status_Finisher.GetTag() })
	{
		ASC->RegisterGameplayTagEvent(DeathTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleEnemyDeathTagChanged, WeakEnemy);
	}
}

void UWarriorStageStatsSubsystem::HandleEnemyDeathTagChanged(const FGameplayTag InTag, const int32 InNewCount, TWeakObjectPtr<AActor> InEnemy)
{
	if (InNewCount <= 0 || !InEnemy.IsValid())
	{
		return;
	}

	FName TagType = NAME_None;
	if (InTag == WarriorGameplayTags::Shared_Status_Death_Finisher || InTag == WarriorGameplayTags::Shared_Status_Finisher)
	{
		TagType = TEXT("Finisher");
	}
	else if (InTag == WarriorGameplayTags::Shared_Status_Death_Knockback)
	{
		TagType = TEXT("Knockback");
	}
	else if (InTag == WarriorGameplayTags::Shared_Status_Death_Normal)
	{
		TagType = TEXT("Normal");
	}

	FName& Stored = EnemyDeathTypes.FindOrAdd(InEnemy);
	Stored = PickDeathType(Stored, TagType);
}

FWarriorAttackKey UWarriorStageStatsSubsystem::MakeAttackKey(const FGameplayEffectContextHandle& InContext) const
{
	const UGameplayAbility* Ability = InContext.GetAbilityInstance_NotReplicated();
	if (!Ability)
	{
		Ability = InContext.GetAbility();
	}

	FWarriorAttackKey Key;
	Key.Ability = FObjectKey(Ability);
	const uint64* Serial = AttackSerials.Find(Key.Ability);
	// 공격 시도 기록(RecordAttackAttempt)이 아직 연결되지 않았으면 같은 프레임의 타격을 한 공격으로 본다.
	Key.Serial = Serial ? *Serial : (GFrameCounter | (1ull << 63));
	return Key;
}

void UWarriorStageStatsSubsystem::HandleEnemySpawned(AActor* InEnemy)
{
	if (!IsRecording() || !InEnemy)
	{
		return;
	}

	++Record.FindOrAddEnemyType(GetTypeName(InEnemy)).Spawned;
	EnemySpawnTimes.Add(InEnemy, GetStageTimeSeconds());
	WatchDeathTags(InEnemy);
}

void UWarriorStageStatsSubsystem::HandleEnemyKilled(AActor* InEnemy, const FName InDeathType)
{
	if (!IsRecording() || !InEnemy)
	{
		return;
	}

	const FName EnemyType = GetTypeName(InEnemy);
	FWarriorEnemyTypeStats& EnemyStats = Record.FindOrAddEnemyType(EnemyType);
	++EnemyStats.Killed;
	if (const float* SpawnTime = EnemySpawnTimes.Find(InEnemy))
	{
		EnemyStats.TotalTimeToKill += FMath::Max(0.0f, GetStageTimeSeconds() - *SpawnTime);
	}

	if (FWarriorWaveRecord* Wave = GetMutableCurrentWave())
	{
		++Wave->Kills;
	}

	// 막타가 플레이어였거나, 데미지 기록이 아직 연결되지 않아 알 수 없으면 플레이어 처치로 센다.
	const FWarriorLastHit* LastHit = LastHits.Find(InEnemy);
	if (!LastHit || LastHit->bByPlayer)
	{
		FWarriorAttackStats& Attack = Record.Stats.Attack;
		++Attack.Kills;
		++Attack.KillsByEnemyType.FindOrAdd(EnemyType);
		// 사망 신호 시점의 태그와 미리 기억해 둔 태그 중 우선순위가 높은 쪽. 둘 다 없으면 Unknown으로 세어 합계가 Kills와 맞게 한다.
		const FName* StoredDeathType = EnemyDeathTypes.Find(InEnemy);
		FName DeathType = PickDeathType(InDeathType, StoredDeathType ? *StoredDeathType : NAME_None);
		if (DeathType.IsNone())
		{
			DeathType = TEXT("Unknown");
		}
		++Attack.KillsByDeathType.FindOrAdd(DeathType);
		if (LastHit)
		{
			FWarriorAttackTrack& Track = AttackTracks.FindOrAdd(LastHit->AttackKey);
			++Track.Kills;
			Attack.MaxKillsPerAttack = FMath::Max(Attack.MaxKillsPerAttack, Track.Kills);
		}
	}

	LastHits.Remove(InEnemy);
	EnemySpawnTimes.Remove(InEnemy);
	EnemyDeathTypes.Remove(InEnemy);
}

void UWarriorStageStatsSubsystem::HandleDamage(const FGameplayEffectContextHandle& InContext, AActor* InTarget, const float InDamage, const float InOverkill, const bool bInFatal)
{
	if (!IsRecording() || !InTarget || (InDamage <= 0.0f && InOverkill <= 0.0f))
	{
		return;
	}

	AActor* Instigator = InContext.GetInstigator();
	const bool bByPlayer = IsPlayerActor(Instigator);
	const FName AbilityName = GetTypeName(InContext.GetAbility());

	// 플레이어 → 적
	if (Cast<AWarriorAICharacter>(InTarget))
	{
		const FWarriorAttackKey AttackKey = MakeAttackKey(InContext);
		FWarriorLastHit& LastHit = LastHits.FindOrAdd(InTarget);
		LastHit.AttackKey = AttackKey;
		LastHit.AbilityName = AbilityName;
		LastHit.bByPlayer = bByPlayer;

		if (bByPlayer)
		{
			FWarriorAttackStats& Attack = Record.Stats.Attack;
			++Attack.HitsDealt;
			Attack.DamageDealt += InDamage;
			Attack.MaxDamageDealt = FMath::Max(Attack.MaxDamageDealt, static_cast<double>(InDamage));
			if (!AbilityName.IsNone())
			{
				Attack.DamageByAbility.FindOrAdd(AbilityName) += InDamage;
			}
			if (InOverkill > 0.0f)
			{
				Record.Stats.AddExtra(WarriorStatTags::Stat_Combat_Damage_Overkill, InOverkill, GetTypeName(InTarget));
			}

			FWarriorAttackTrack& Track = AttackTracks.FindOrAdd(AttackKey);
			if (!Track.bLanded)
			{
				Track.bLanded = true;
				++Attack.AttacksLanded;
			}
		}
		return;
	}

	// 적(또는 환경) → 플레이어
	if (IsPlayerActor(InTarget))
	{
		FWarriorDefenseStats& Defense = Record.Stats.Defense;
		++Defense.HitsTaken;
		Defense.DamageTaken += InDamage;
		MarkPlayerHitThisWave();

		const FName EnemyType = Cast<AWarriorAICharacter>(Instigator) ? GetTypeName(Instigator) : NAME_None;
		if (!EnemyType.IsNone())
		{
			Defense.DamageTakenByEnemyType.FindOrAdd(EnemyType) += InDamage;
			Record.FindOrAddEnemyType(EnemyType).DamageToPlayer += InDamage;
		}

		if (const AWarriorBaseCharacter* Character = Cast<AWarriorBaseCharacter>(InTarget))
		{
			const UWarriorAttributeSet* AttributeSet = Character->GetWarriorAttributeSet();
			if (AttributeSet && AttributeSet->GetMaxHealth() > 0.0f)
			{
				Record.Stats.AddExtra(WarriorStatTags::Stat_Defense_Health_MinRatio, AttributeSet->GetCurrentHealth() / AttributeSet->GetMaxHealth());
			}
		}

		if (bInFatal)
		{
			++Defense.Deaths;
			FWarriorDeathRecord& Death = Record.Deaths.AddDefaulted_GetRef();
			Death.KillerEnemyType = EnemyType;
			Death.KillerAbility = AbilityName;
			Death.WaveNumber = GetCurrentWaveNumber();
			Death.TimeSeconds = GetStageTimeSeconds();
			if (!EnemyType.IsNone())
			{
				++Record.FindOrAddEnemyType(EnemyType).PlayerKills;
			}
		}
	}
}

void UWarriorStageStatsSubsystem::HandleBalanceDamage(const FGameplayEffectContextHandle& InContext, AActor* InTarget, const float InAmount)
{
	if (!IsRecording() || InAmount <= 0.0f || !Cast<AWarriorAICharacter>(InTarget) || !IsPlayerActor(InContext.GetInstigator()))
	{
		return;
	}

	Record.Stats.AddExtra(WarriorStatTags::Stat_Combat_Balance_Dealt, InAmount, GetTypeName(InTarget));
}

void UWarriorStageStatsSubsystem::HandleHeal(const FGameplayEffectContextHandle& InContext, AActor* InTarget, const float InHealed, const float InOverheal)
{
	if (!IsRecording() || !IsPlayerActor(InTarget))
	{
		return;
	}

	Record.Stats.Heal.HealAmount += FMath::Max(0.0f, InHealed);
	Record.Stats.Heal.Overheal += FMath::Max(0.0f, InOverheal);
}

void UWarriorStageStatsSubsystem::HandleAttackAttempt(const UGameplayAbility* InAbility)
{
	if (!IsRecording() || !InAbility || !IsPlayerActor(InAbility->GetAvatarActorFromActorInfo()))
	{
		return;
	}

	++Record.Stats.Attack.AttackAttempts;
	// 같은 어빌리티 인스턴스가 다시 발동되어도 다른 공격으로 구분되도록 번호를 올린다.
	++AttackSerials.FindOrAdd(FObjectKey(InAbility));
}

void UWarriorStageStatsSubsystem::HandleGoldEarned(const int32 InAmount, const FName InSource)
{
	if (!IsRecording() || InAmount <= 0)
	{
		return;
	}

	Record.Stats.Economy.GoldEarned += InAmount;
	Record.Stats.Economy.GoldEarnedBySource.FindOrAdd(InSource.IsNone() ? FName(TEXT("Unknown")) : InSource) += InAmount;
}

void UWarriorStageStatsSubsystem::HandleGoldSpent(const int32 InAmount)
{
	if (!IsRecording() || InAmount <= 0)
	{
		return;
	}

	Record.Stats.Economy.GoldSpent += InAmount;
}

void UWarriorStageStatsSubsystem::HandlePotionUsed(const FName InItemId)
{
	if (!IsRecording())
	{
		return;
	}

	++Record.Stats.Heal.PotionsUsed;
	if (!InItemId.IsNone())
	{
		++Record.Stats.Heal.PotionsUsedByItem.FindOrAdd(InItemId);
	}
}

void UWarriorStageStatsSubsystem::HandlePurchase(const FName InItemId, const int32 InCount, const int32 InGoldSpent)
{
	if (!IsRecording() || InItemId.IsNone() || InCount <= 0)
	{
		return;
	}

	// 골드 사용량은 인벤토리 골드 감소로 따로 기록하므로(S1-6) 여기서는 구매 이력만 남긴다.
	FWarriorItemPurchaseStats& Purchase = Record.Stats.Economy.PurchasesByItem.FindOrAdd(InItemId);
	Purchase.Count += InCount;
	Purchase.GoldSpent += FMath::Max(0, InGoldSpent);

	FWarriorPurchaseRecord& PurchaseRecord = Record.Purchases.AddDefaulted_GetRef();
	PurchaseRecord.ItemId = InItemId;
	PurchaseRecord.Count = InCount;
	PurchaseRecord.GoldSpent = FMath::Max(0, InGoldSpent);
	PurchaseRecord.WaveNumber = GetCurrentWaveNumber();
	PurchaseRecord.TimeSeconds = GetStageTimeSeconds();
}

void UWarriorStageStatsSubsystem::HandleStat(const FGameplayTag& InStatTag, const double InValue, const FName InDimensionKey)
{
	if (!IsRecording())
	{
		return;
	}

	Record.Stats.AddExtra(InStatTag, InValue, InDimensionKey);
}
