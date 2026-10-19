// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "AIGameplayAbility_Dodge.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EWarriorDodgeDirection : uint8
{
	Forward,
	Backward,
	Left,
	Right
};

/**
 * 플레이어가 공격을 시작하면(AI.Event.IncomingAttack) 확률적으로 회피하는 AI 어빌리티. 엘리트 시작 데이터에만 넣는다.
 * 반응 지연 후 공격자 반대쪽·좌우 중 내비메시가 열린 방향을 골라 해당 방향 몽타주를 재생하고,
 * 시작부터 InvulnerableDuration 동안 Shared.Status.Dodge를 붙여 공격을 피한다.
 * 회피 중(AI.Status.Dodging)에는 공격 어빌리티가 발동하지 않는다.
 * BT에서 직접 발동할 수도 있다 (거리 조건 백스텝 등. 이때는 블랙보드 TargetActor 기준으로 피함).
 * 원거리 엘리트 백스텝: SideWeight 0, Bow 백스텝 몽타주, FollowUpAbilityTag = 속사
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_Dodge : public UAIGameplayAbility
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_Dodge();

protected:
	//~ Begin GameplayAbility Interface.
	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 공격 한 번마다 회피할 확률
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DodgeChance = 0.35f;

	// 회피 후 다시 회피할 수 있기까지의 시간
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float DodgeCooldown = 3.f;

	// 공격 시작부터 회피 동작까지의 지연 (최소, 최대). 너무 빠르면 미래를 읽는 것처럼 보임
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "s"))
	FVector2D ReactionDelay = FVector2D(0.1f, 0.25f);

	// 회피 동작 시작부터 무적(Shared.Status.Dodge)인 시간
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float InvulnerableDuration = 0.45f;

	// 회피 방향으로 이만큼 내비메시가 이어져 있어야 그 방향을 고름 (벽·낭떠러지 쪽으로 구르지 않게)
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "cm"))
	float ClearanceDistance = 300.f;

	// 방향 선택 가중치. 공격자 반대쪽(뒤)과 좌우
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0"))
	float BackwardWeight = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.0"))
	float SideWeight = 1.f;

	// 방향별 회피 몽타주 (AI 기준 방향). 없는 방향은 Backward로 대체
	UPROPERTY(EditDefaultsOnly, Category = "Dodge|Montage")
	TMap<EWarriorDodgeDirection, TObjectPtr<UAnimMontage>> DodgeMontages;

private:
	UFUNCTION()
	void OnReactionDelayFinished();

	UFUNCTION()
	void OnInvulnerabilityFinished();

	UFUNCTION()
	void OnDodgeMontageFinished();

	UFUNCTION()
	void OnDodgeMontageInterrupted();

	// 공격자 기준 뒤·좌우 중 열린 방향을 가중치로 고름. 열린 방향이 없으면 false
	bool ChooseDodgeDirection(const AActor* Avatar, const AActor* InAttacker, FVector& OutWorldDirection) const;
	bool HasClearance(const FVector& Start, const FVector& Direction) const;
	UAnimMontage* FindMontageForWorldDirection(const AActor* Avatar, const FVector& WorldDirection) const;
	bool IsAttacking(const UAbilitySystemComponent* ASC) const;
	void SetInvulnerable(bool bEnable);

	TWeakObjectPtr<const AActor> Attacker;
	double LastDodgeTime = -1.0e9;
	bool bInvulnerable = false;
};
