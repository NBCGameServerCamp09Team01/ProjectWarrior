#include "WarriorWaveSpawner.h"

#include "Engine/World.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"

AWarriorWaveSpawner::AWarriorWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AWarriorWaveSpawner::StartWaveFromData(const FWarriorStageWaveData& InWaveData)
{
	CompactAliveEnemies();

	if (bWaveActive || !AliveEnemies.IsEmpty())
	{
		UE_LOG(LogProjectWarrior, Warning,
			TEXT("WaveSpawner %s rejected overlapping waves (%d tracked enemies). Call StopSpawning and remove remaining enemies before restarting."),
			*GetName(), AliveEnemies.Num());
		return false;
	}

	StopSpawning();
	NextRequestIndex = 0;
	SpawnedEnemyCount = 0;
	CurrentSpawnAttempts = 0;
	bWaveClearReported = false;
	CurrentDropModifier = InWaveData.DropModifier;
	if (!BuildPendingRequests(InWaveData))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("WaveSpawner %s rejected empty or invalid wave data. No clear event will be emitted."), *GetName());
		return false;
	}

	bWaveActive = true;
	const uint64 Generation = WaveGeneration;
	NotifyEnemyCountChanged();
	// 호출자가 웨이브 상태 진입을 마칠 수 있도록 첫 스폰을 다음 틱으로 미룬다.
	if (bWaveActive && Generation == WaveGeneration)
	{
		SpawnTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ProcessNextSpawnRequest);
	}
	return true;
}

void AWarriorWaveSpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	++WaveGeneration;
	PendingRequests.Reset();
	NextRequestIndex = 0;
	bWaveActive = false;
}

void AWarriorWaveSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	UnbindTrackedEnemies();
	AliveEnemies.Reset();
	EnemyRewards.Reset();
	Super::EndPlay(EndPlayReason);
}

void AWarriorWaveSpawner::UnbindTrackedEnemies()
{
	for (const TWeakObjectPtr<AWarriorAICharacter>& Enemy : AliveEnemies)
	{
		UnbindEnemy(Enemy.Get());
	}
}

void AWarriorWaveSpawner::UnbindEnemy(AWarriorAICharacter* Enemy)
{
	if (IsValid(Enemy))
	{
		Enemy->OnCharacterDied.RemoveDynamic(this, &ThisClass::HandleSpawnedEnemyDied);
		Enemy->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSpawnedEnemyDestroyed);
	}
}

int32 AWarriorWaveSpawner::GetAliveEnemyCount() const
{
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AWarriorAICharacter>& Enemy : AliveEnemies)
	{
		if (Enemy.IsValid())
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

bool AWarriorWaveSpawner::HasAliveEnemies() const
{
	return GetAliveEnemyCount() > 0 || PendingRequests.IsValidIndex(NextRequestIndex);
}

bool AWarriorWaveSpawner::BuildPendingRequests(const FWarriorStageWaveData& InWaveData)
{
	PendingRequests.Reset();
	// 웨이브를 시작하기 전에 모든 항목을 검증한다. 잘못된 항목을 조용히 건너뛰지 않는다.
	if (InWaveData.Enemies.IsEmpty())
	{
		return false;
	}
	for (const FWarriorWaveEnemySpawnData& EnemyData : InWaveData.Enemies)
	{
		if (!EnemyData.EnemyClass || EnemyData.Count <= 0 || EnemyData.EnemyClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return false;
		}
	}

	for (const FWarriorWaveEnemySpawnData& EnemyData : InWaveData.Enemies)
	{
		for (int32 SpawnIndex = 0; SpawnIndex < EnemyData.Count; ++SpawnIndex)
		{
			FWarriorPendingWaveSpawnRequest& Request = PendingRequests.AddDefaulted_GetRef();
			Request.EnemyClass = EnemyData.EnemyClass;
			Request.SpawnGroup = EnemyData.SpawnGroup;
			Request.GoldReward = FMath::Max(0, EnemyData.GoldReward);
			Request.GoldDropChance = FMath::Clamp(EnemyData.GoldDropChance, 0.0f, 1.0f);
		}
	}
	return true;
}

void AWarriorWaveSpawner::ProcessNextSpawnRequest()
{
	if (!bWaveActive)
	{
		return;
	}

	if (!PendingRequests.IsValidIndex(NextRequestIndex))
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		TryReportWaveCleared();
		return;
	}

	const uint64 Generation = WaveGeneration;
	const FWarriorPendingWaveSpawnRequest SpawnRequest = PendingRequests[NextRequestIndex];
	const bool bSpawned = TrySpawnEnemy(SpawnRequest);
	if (!bWaveActive || Generation != WaveGeneration)
	{
		return;
	}
	if (bSpawned)
	{
		++NextRequestIndex;
		CurrentSpawnAttempts = 0;
		NotifyEnemyCountChanged();
		if (!bWaveActive || Generation != WaveGeneration)
		{
			return;
		}
	}
	else if (++CurrentSpawnAttempts >= FMath::Max(1, MaxSpawnAttempts))
	{
		UE_LOG(LogProjectWarrior, Error,
			TEXT("[Wave] %s skipped request %d, group '%s', after %d failed attempts. Check spawn points."),
			*GetName(), NextRequestIndex, *SpawnRequest.SpawnGroup.ToString(), CurrentSpawnAttempts);

		++NextRequestIndex;
		CurrentSpawnAttempts = 0;
		NotifyEnemyCountChanged();
		if (!bWaveActive || Generation != WaveGeneration)
		{
			return;
		}
	}

	if (PendingRequests.IsValidIndex(NextRequestIndex))
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&ThisClass::ProcessNextSpawnRequest,
			FMath::Max(bSpawned ? SpawnInterval : SpawnRetryInterval, 0.01f),
			false);
	}
	else
	{
		TryReportWaveCleared();
	}
}

bool AWarriorWaveSpawner::TrySpawnEnemy(const FWarriorPendingWaveSpawnRequest& SpawnRequest)
{
	if (!SpawnRequest.EnemyClass)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.OverrideLevel = GetLevel();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	TArray<int32> Candidates;
	for (int32 Index = 0; Index < SpawnPoints.Num(); ++Index)
	{
		const FWarriorWaveSpawnPointData& Point = SpawnPoints[Index];
		if (IsValid(Point.SpawnPoint) && Point.SpawnGroup == SpawnRequest.SpawnGroup && FMath::IsFinite(Point.Weight) && Point.Weight > 0.0f)
		{
			Candidates.Add(Index);
		}
	}

	while (!Candidates.IsEmpty())
	{
		const int32 CandidateIndex = SelectWeightedSpawnPoint(Candidates);
		const FWarriorWaveSpawnPointData& Point = SpawnPoints[Candidates[CandidateIndex]];
		Candidates.RemoveAtSwap(CandidateIndex);
		FTransform SpawnTransform;
		if (!FindSafeSpawnTransform(Point, SpawnRequest.EnemyClass, SpawnTransform))
		{
			continue;
		}
		AWarriorAICharacter* Enemy = GetWorld()->SpawnActor<AWarriorAICharacter>(SpawnRequest.EnemyClass, SpawnTransform, SpawnParameters);
		if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed())
		{
			continue;
		}
		AliveEnemies.Add(Enemy);
		EnemyRewards.Add(Enemy, FWarriorWaveEnemyReward{ SpawnRequest.GoldReward, SpawnRequest.GoldDropChance });
		++SpawnedEnemyCount;
		Enemy->OnCharacterDied.AddUniqueDynamic(this, &ThisClass::HandleSpawnedEnemyDied);
		Enemy->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleSpawnedEnemyDestroyed);
		return true;
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("WaveSpawner %s: attempt %d failed for group '%s'; keeping request queued."),
		*GetName(), CurrentSpawnAttempts + 1, *SpawnRequest.SpawnGroup.ToString());
	return false;
}

bool AWarriorWaveSpawner::FindSafeSpawnTransform(
	const FWarriorWaveSpawnPointData& SpawnPoint,
	const TSubclassOf<AWarriorAICharacter> EnemyClass,
	FTransform& OutTransform) const
{
	if (!IsValid(SpawnPoint.SpawnPoint) || !EnemyClass)
	{
		return false;
	}

	FVector SpawnLocation = SpawnPoint.SpawnPoint->GetActorLocation();
	FRotator SpawnRotation = SpawnPoint.SpawnPoint->GetActorRotation();
	const AWarriorAICharacter* EnemyDefaultObject = EnemyClass->GetDefaultObject<AWarriorAICharacter>();

	if (!GetWorld()->FindTeleportSpot(EnemyDefaultObject, SpawnLocation, SpawnRotation))
	{
		return false;
	}

	// 마커 크기는 에디터 편집용이므로 생성되는 캐릭터 캡슐의 크기에 적용하지 않는다.
	OutTransform = FTransform(SpawnRotation, SpawnLocation, EnemyDefaultObject->GetActorScale3D());
	return true;
}

int32 AWarriorWaveSpawner::SelectWeightedSpawnPoint(const TArray<int32>& Candidates) const
{
	double TotalWeight = 0.0;
	for (const int32 Index : Candidates)
	{
		TotalWeight += SpawnPoints[Index].Weight;
	}

	double Selection = FMath::FRand() * TotalWeight;
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		Selection -= SpawnPoints[Candidates[CandidateIndex]].Weight;
		if (Selection <= 0.0)
		{
			return CandidateIndex;
		}
	}

	return Candidates.Num() - 1;
}

void AWarriorWaveSpawner::NotifyEnemyCountChanged()
{
	CompactAliveEnemies();
	OnEnemyCountChanged.Broadcast(AliveEnemies.Num(), SpawnedEnemyCount);
}

void AWarriorWaveSpawner::TryReportWaveCleared()
{
	CompactAliveEnemies();

	if (!bWaveActive || bWaveClearReported || PendingRequests.IsValidIndex(NextRequestIndex) || !AliveEnemies.IsEmpty())
	{
		return;
	}

	bWaveClearReported = true;
	bWaveActive = false;
	OnWaveCleared.Broadcast();
}

void AWarriorWaveSpawner::CompactAliveEnemies()
{
	for (auto EnemyIt = AliveEnemies.CreateIterator(); EnemyIt; ++EnemyIt)
	{
		if (!EnemyIt->IsValid())
		{
			EnemyIt.RemoveCurrent();
		}
	}
	for (auto RewardIt = EnemyRewards.CreateIterator(); RewardIt; ++RewardIt)
	{
		if (!RewardIt.Key().IsValid())
		{
			RewardIt.RemoveCurrent();
		}
	}
}

bool AWarriorWaveSpawner::RemoveTrackedEnemy(AWarriorAICharacter* Enemy, const TCHAR* Reason)
{
	// 사망 → Destroy 순서로 두 신호가 모두 와도 한 번만 집계한다.
	if (!Enemy || AliveEnemies.Remove(Enemy) == 0)
	{
		return false;
	}

	// 클리어 알림보다 먼저 남겨야 로그 순서가 실제 순서와 맞는다.
	UE_LOG(LogProjectWarrior, Log, TEXT("[Wave] %s: %s %s. Alive %d, remaining spawns %d"),
		*GetName(), *GetNameSafe(Enemy), Reason, GetAliveEnemyCount(), GetRemainingSpawnCount());

	EnemyRewards.Remove(Enemy);
	UnbindEnemy(Enemy);
	NotifyEnemyCountChanged();
	TryReportWaveCleared();
	return true;
}

void AWarriorWaveSpawner::HandleSpawnedEnemyDied(AWarriorBaseCharacter* DeadCharacter)
{
	AWarriorAICharacter* DeadEnemy = Cast<AWarriorAICharacter>(DeadCharacter);
	if (!DeadEnemy || !AliveEnemies.Contains(DeadEnemy))
	{
		return;
	}

	// 웨이브 클리어 알림 전에 지급해서, 클리어 시점에 골드가 이미 반영되어 있게 한다.
	GrantEnemyReward(DeadEnemy);
	RemoveTrackedEnemy(DeadEnemy, TEXT("died"));
}

int32 AWarriorWaveSpawner::GrantEnemyReward(AWarriorAICharacter* Enemy)
{
	const FWarriorWaveEnemyReward* Reward = EnemyRewards.Find(Enemy);
	if (!Reward || Reward->GoldReward <= 0)
	{
		return 0;
	}

	const float DropChance = FMath::Clamp(Reward->GoldDropChance * CurrentDropModifier.GoldDropChanceMultiplier, 0.0f, 1.0f);
	if (DropChance <= 0.0f || (DropChance < 1.0f && FMath::FRand() >= DropChance))
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Wave] %s: no gold from %s (chance %.2f)"), *GetName(), *GetNameSafe(Enemy), DropChance);
		return 0;
	}

	const int32 Gold = FMath::Max(0, FMath::RoundToInt(Reward->GoldReward * CurrentDropModifier.GoldAmountMultiplier));
	if (Gold <= 0)
	{
		return 0;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Wave] %s: Gold +%d from %s (chance %.2f)"), *GetName(), Gold, *GetNameSafe(Enemy), DropChance);
	OnEnemyRewarded.Broadcast(Enemy, Gold);
	GiveGoldToPlayer(Gold);
	return Gold;
}

void AWarriorWaveSpawner::GiveGoldToPlayer(const int32 InGold) const
{
	// MVP는 싱글 플레이 기준으로 첫 번째 플레이어에게 지급한다. 막타 기준 지급은 가해자 정보가 생긴 뒤 검토.
	const APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const AWarriorPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AWarriorPlayerState>() : nullptr;
	UPlayerInventoryComponent* Inventory = PlayerState ? PlayerState->GetPlayerInventoryComponent() : nullptr;
	if (!Inventory)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Wave] %s: no player inventory. %d gold was not given."), *GetName(), InGold);
		return;
	}

	Inventory->AddGold(InGold);
}

void AWarriorWaveSpawner::HandleSpawnedEnemyDestroyed(AActor* DestroyedActor)
{
	RemoveTrackedEnemy(Cast<AWarriorAICharacter>(DestroyedActor), TEXT("destroyed without death signal"));
}
