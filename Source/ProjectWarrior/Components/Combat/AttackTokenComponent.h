// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AttackTokenComponent.generated.h"

class AWarriorBaseCharacter;

// 토큰 보유 기록. 누가 어느 풀에서 몇 개를 언제 가져갔는지
USTRUCT()
struct FWarriorAttackTokenLease
{
	GENERATED_BODY()

	FGameplayTag Pool;
	int32 Cost = 0;
	double AcquiredTime = 0.0;
	// 같은 보유자가 받은 횟수 (BT 데코레이터 + 공격 어빌리티). 모두 반납해야 토큰이 풀로 돌아감
	int32 RefCount = 0;
};

/**
 * 공격받는 쪽(플레이어)이 들고 있는 공격 토큰 풀.
 * 적 AI는 공격 어빌리티를 시작할 때 토큰을 받고, 끝날 때 반납한다. 풀(AI.AttackToken.*)마다 동시에 나갈 수 있는 수가 정해져 있다.
 * 반납 누락에 대비해 보유자 사망, 보유자 소멸, 최대 보유 시간 초과 시 자동으로 회수한다.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PROJECTWARRIOR_API UAttackTokenComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttackTokenComponent();

	// 지금 토큰을 받을 수 있는지. 이미 같은 풀의 토큰을 들고 있으면 true
	UFUNCTION(BlueprintPure, Category = "Warrior|AttackToken")
	bool CanAcquire(const AActor* Requester, FGameplayTag Pool, int32 Cost = 1) const;

	// 토큰을 받는다. 이미 같은 풀의 토큰을 들고 있으면 새로 차감하지 않고 받은 횟수만 늘림. TryAcquire마다 Release를 한 번씩 호출해야 함
	UFUNCTION(BlueprintCallable, Category = "Warrior|AttackToken")
	bool TryAcquire(AActor* Requester, FGameplayTag Pool, int32 Cost = 1);

	// 받은 횟수를 하나 줄이고, 0이 되면 토큰을 풀로 돌려준다. 들고 있지 않으면 아무 일도 하지 않음
	UFUNCTION(BlueprintCallable, Category = "Warrior|AttackToken")
	void Release(AActor* Requester);

	UFUNCTION(BlueprintCallable, Category = "Warrior|AttackToken")
	void ReleaseAll();

	// 풀에 남은 토큰 수. 등록되지 않은 풀이면 0
	UFUNCTION(BlueprintPure, Category = "Warrior|AttackToken")
	int32 GetAvailableTokens(FGameplayTag Pool) const;

	UFUNCTION(BlueprintPure, Category = "Warrior|AttackToken")
	bool IsHolding(const AActor* Requester) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 풀별 최대 토큰 수. 여기 없는 풀을 요청하면 제한 없이 허용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackToken", meta = (Categories = "AI.AttackToken"))
	TMap<FGameplayTag, int32> PoolCapacities;

	// 반납 후 같은 AI가 다시 토큰을 받기까지의 시간. 한 AI가 연달아 독점하는 것을 막음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackToken", meta = (ClampMin = "0.0", Units = "s"))
	float ReacquireCooldown = 0.5f;

	// 이 시간이 지나도 반납되지 않은 토큰은 회수 (반납 누락 대비). BT 데코레이터는 접근부터 들고 있으므로 접근+공격 시간보다 길게. 0이면 회수하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AttackToken", meta = (ClampMin = "0.0", Units = "s"))
	float MaxHoldTime = 10.f;

private:
	int32 GetUsedTokens(FGameplayTag Pool) const;
	bool IsOnReacquireCooldown(const AActor* Requester) const;
	bool IsLeaseExpired(const FWarriorAttackTokenLease& Lease) const;
	// 소멸한 보유자와 최대 보유 시간을 넘긴 토큰을 정리
	void PruneLeases();
	void RemoveLease(AActor* Requester, bool bStartCooldown);
	double GetNow() const;
	// pw.AttackToken.Debug 1일 때 매 프레임 호출
	void DrawDebugTokens() const;

	UFUNCTION()
	void HandleHolderDied(AWarriorBaseCharacter* DeadCharacter);

	UFUNCTION()
	void HandleOwnerDied(AWarriorBaseCharacter* DeadCharacter);

	TMap<TWeakObjectPtr<AActor>, FWarriorAttackTokenLease> Leases;
	TMap<TWeakObjectPtr<AActor>, double> LastReleaseTimes;
};
