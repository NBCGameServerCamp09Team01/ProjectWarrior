// Fill out your copyright notice in the Description page of Project Settings.


#include "AttackTokenComponent.h"
#include "ProjectWarrior/Characters/WarriorBaseCharacter.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// const 포인터로 TWeakObjectPtr 키를 조회하기 위한 변환
	TWeakObjectPtr<AActor> MakeHolderKey(const AActor* Actor)
	{
		return TWeakObjectPtr<AActor>(const_cast<AActor*>(Actor));
	}

#if ENABLE_DRAW_DEBUG
	TAutoConsoleVariable<int32> CVarAttackTokenDebug(
		TEXT("pw.AttackToken.Debug"),
		0,
		TEXT("공격 토큰 디버그 표시. 0: 끔, 1: 대상 머리 위에 풀별 사용량, 보유자 연결선과 보유 시간, 재획득 대기 표시"),
		ECVF_Cheat);
#endif
}

UAttackTokenComponent::UAttackTokenComponent()
{
	// 디버그 표시용 Tick. 표시를 끄면 바로 반환하며, 디버그 그리기가 없는 빌드에서는 Tick하지 않음
	PrimaryComponentTick.bCanEverTick = ENABLE_DRAW_DEBUG;
	PrimaryComponentTick.bStartWithTickEnabled = ENABLE_DRAW_DEBUG;

	PoolCapacities.Add(WarriorGameplayTags::AI_AttackToken_Melee, 2);
	PoolCapacities.Add(WarriorGameplayTags::AI_AttackToken_Range, 1);
}

void UAttackTokenComponent::BeginPlay()
{
	Super::BeginPlay();

	// 대상이 죽으면 들고 있던 토큰을 모두 회수
	if (AWarriorBaseCharacter* OwnerCharacter = Cast<AWarriorBaseCharacter>(GetOwner()))
	{
		OwnerCharacter->OnCharacterDied.AddUniqueDynamic(this, &ThisClass::HandleOwnerDied);
	}
}

void UAttackTokenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ENABLE_DRAW_DEBUG
	if (CVarAttackTokenDebug.GetValueOnGameThread() > 0)
	{
		DrawDebugTokens();
	}
#endif
}

void UAttackTokenComponent::DrawDebugTokens() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return;
	}

	// 상태를 바꾸지 않도록 정리(PruneLeases) 없이 그대로 표시. 만료된 토큰은 따로 표시
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const double Now = GetNow();

	FString Summary;
	for (const TPair<FGameplayTag, int32>& Pool : PoolCapacities)
	{
		const FString PoolName = Pool.Key.ToString().RightChop(FString(TEXT("AI.AttackToken.")).Len());
		Summary += FString::Printf(TEXT("%s %d/%d  "), *PoolName, GetUsedTokens(Pool.Key), Pool.Value);
	}
	DrawDebugString(World, OwnerLocation + FVector(0.f, 0.f, 130.f), Summary, nullptr, FColor::White, 0.f, true, 1.2f);

	for (const TPair<TWeakObjectPtr<AActor>, FWarriorAttackTokenLease>& Pair : Leases)
	{
		const AActor* Holder = Pair.Key.Get();
		if (!Holder)
		{
			continue;
		}

		const bool bExpired = IsLeaseExpired(Pair.Value);
		const FColor Color = bExpired ? FColor::Magenta
			: Pair.Value.Pool == WarriorGameplayTags::AI_AttackToken_Range ? FColor::Cyan
			: FColor::Red;
		const FVector HolderLocation = Holder->GetActorLocation();

		DrawDebugLine(World, HolderLocation, OwnerLocation, Color, false, -1.f, 0, 2.f);
		DrawDebugString(World, HolderLocation + FVector(0.f, 0.f, 110.f),
			FString::Printf(TEXT("%s x%d  %.1fs%s"), *Pair.Value.Pool.ToString().RightChop(FString(TEXT("AI.AttackToken.")).Len()),
				Pair.Value.Cost, Now - Pair.Value.AcquiredTime, bExpired ? TEXT("  EXPIRED") : TEXT("")),
			nullptr, Color, 0.f, true);
	}

	for (const TPair<TWeakObjectPtr<AActor>, double>& Pair : LastReleaseTimes)
	{
		const AActor* Holder = Pair.Key.Get();
		const double Remaining = ReacquireCooldown - (Now - Pair.Value);
		if (Holder && Remaining > 0.0 && !Leases.Contains(Pair.Key))
		{
			DrawDebugString(World, Holder->GetActorLocation() + FVector(0.f, 0.f, 110.f),
				FString::Printf(TEXT("cooldown %.1fs"), Remaining), nullptr, FColor::Silver, 0.f, true);
		}
	}
#endif
}

void UAttackTokenComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseAll();

	Super::EndPlay(EndPlayReason);
}

bool UAttackTokenComponent::CanAcquire(const AActor* Requester, FGameplayTag Pool, int32 Cost) const
{
	if (!Requester || !Pool.IsValid())
	{
		return false;
	}

	if (const FWarriorAttackTokenLease* Lease = Leases.Find(MakeHolderKey(Requester)))
	{
		if (!IsLeaseExpired(*Lease))
		{
			// 한 AI는 한 번에 한 풀의 토큰만 들 수 있음
			return Lease->Pool == Pool;
		}
	}

	if (IsOnReacquireCooldown(Requester))
	{
		return false;
	}

	const int32* Capacity = PoolCapacities.Find(Pool);
	if (!Capacity)
	{
		return true;
	}

	return GetUsedTokens(Pool) + FMath::Max(Cost, 1) <= *Capacity;
}

bool UAttackTokenComponent::TryAcquire(AActor* Requester, FGameplayTag Pool, int32 Cost)
{
	PruneLeases();

	if (!CanAcquire(Requester, Pool, Cost))
	{
		return false;
	}

	// 이미 들고 있으면(BT 데코레이터가 받아 둔 토큰을 어빌리티가 다시 요청 등) 받은 횟수만 늘림
	if (FWarriorAttackTokenLease* ExistingLease = Leases.Find(Requester))
	{
		++ExistingLease->RefCount;
		return true;
	}

	FWarriorAttackTokenLease& Lease = Leases.Add(Requester);
	Lease.Pool = Pool;
	Lease.Cost = FMath::Max(Cost, 1);
	Lease.AcquiredTime = GetNow();
	Lease.RefCount = 1;

	// 반납 전에 보유자가 죽으면 회수
	if (AWarriorBaseCharacter* HolderCharacter = Cast<AWarriorBaseCharacter>(Requester))
	{
		HolderCharacter->OnCharacterDied.AddUniqueDynamic(this, &ThisClass::HandleHolderDied);
	}

	UE_LOG(LogProjectWarrior, Verbose, TEXT("[AttackToken] %s acquired %s (%d). Remaining %d"),
		*GetNameSafe(Requester), *Pool.ToString(), Lease.Cost, GetAvailableTokens(Pool));

	return true;
}

void UAttackTokenComponent::Release(AActor* Requester)
{
	FWarriorAttackTokenLease* Lease = Requester ? Leases.Find(Requester) : nullptr;
	if (!Lease)
	{
		return;
	}

	// 다른 쪽(BT 데코레이터 또는 공격 어빌리티)이 아직 쓰는 중이면 유지
	if (--Lease->RefCount > 0)
	{
		return;
	}

	RemoveLease(Requester, true);
}

void UAttackTokenComponent::ReleaseAll()
{
	TArray<TWeakObjectPtr<AActor>> Holders;
	Leases.GetKeys(Holders);

	for (const TWeakObjectPtr<AActor>& Holder : Holders)
	{
		if (AWarriorBaseCharacter* HolderCharacter = Cast<AWarriorBaseCharacter>(Holder.Get()))
		{
			HolderCharacter->OnCharacterDied.RemoveDynamic(this, &ThisClass::HandleHolderDied);
		}
	}

	Leases.Reset();
	LastReleaseTimes.Reset();
}

int32 UAttackTokenComponent::GetAvailableTokens(FGameplayTag Pool) const
{
	const int32* Capacity = PoolCapacities.Find(Pool);
	return Capacity ? FMath::Max(*Capacity - GetUsedTokens(Pool), 0) : 0;
}

bool UAttackTokenComponent::IsHolding(const AActor* Requester) const
{
	const FWarriorAttackTokenLease* Lease = Leases.Find(MakeHolderKey(Requester));
	return Lease && !IsLeaseExpired(*Lease);
}

int32 UAttackTokenComponent::GetUsedTokens(FGameplayTag Pool) const
{
	int32 Used = 0;

	for (const TPair<TWeakObjectPtr<AActor>, FWarriorAttackTokenLease>& Pair : Leases)
	{
		if (Pair.Key.IsValid() && Pair.Value.Pool == Pool && !IsLeaseExpired(Pair.Value))
		{
			Used += Pair.Value.Cost;
		}
	}

	return Used;
}

bool UAttackTokenComponent::IsOnReacquireCooldown(const AActor* Requester) const
{
	const double* LastReleaseTime = LastReleaseTimes.Find(MakeHolderKey(Requester));
	return LastReleaseTime && GetNow() - *LastReleaseTime < ReacquireCooldown;
}

bool UAttackTokenComponent::IsLeaseExpired(const FWarriorAttackTokenLease& Lease) const
{
	return MaxHoldTime > 0.f && GetNow() - Lease.AcquiredTime >= MaxHoldTime;
}

void UAttackTokenComponent::PruneLeases()
{
	TArray<AActor*> ExpiredHolders;

	for (auto It = Leases.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
		else if (IsLeaseExpired(It.Value()))
		{
			ExpiredHolders.Add(It.Key().Get());
		}
	}

	for (AActor* Holder : ExpiredHolders)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[AttackToken] %s held %s token over %.1fs. Reclaimed."),
			*GetNameSafe(Holder), *GetNameSafe(GetOwner()), MaxHoldTime);
		RemoveLease(Holder, false);
	}

	const double Now = GetNow();
	for (auto It = LastReleaseTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() >= ReacquireCooldown)
		{
			It.RemoveCurrent();
		}
	}
}

void UAttackTokenComponent::RemoveLease(AActor* Requester, bool bStartCooldown)
{
	if (!Requester || !Leases.Remove(Requester))
	{
		return;
	}

	if (AWarriorBaseCharacter* HolderCharacter = Cast<AWarriorBaseCharacter>(Requester))
	{
		HolderCharacter->OnCharacterDied.RemoveDynamic(this, &ThisClass::HandleHolderDied);
	}

	if (bStartCooldown && ReacquireCooldown > 0.f)
	{
		LastReleaseTimes.Add(Requester, GetNow());
	}

	UE_LOG(LogProjectWarrior, Verbose, TEXT("[AttackToken] %s released token"), *GetNameSafe(Requester));
}

double UAttackTokenComponent::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UAttackTokenComponent::HandleHolderDied(AWarriorBaseCharacter* DeadCharacter)
{
	RemoveLease(DeadCharacter, false);
}

void UAttackTokenComponent::HandleOwnerDied(AWarriorBaseCharacter* DeadCharacter)
{
	ReleaseAll();
}
