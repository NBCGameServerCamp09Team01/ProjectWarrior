// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorAICharacter.h"
#include "GameplayEffectTypes.h"
#include "WarriorBossCharacter.generated.h"

class UBossPatternComponent;
class UGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorBossPhaseChanged, int32, OldPhase, int32, NewPhase);

/**
 * 보스 전용 AI 캐릭터. 스폰·사망·보상은 AWarriorAICharacter 흐름을 그대로 사용하고,
 * 패턴 선택(UBossPatternComponent)과 페이즈 상태를 추가로 가진다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorBossCharacter : public AWarriorAICharacter
{
	GENERATED_BODY()

public:
	AWarriorBossCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	int32 GetMaxPhase() const { return MaxPhase; }

	// 1 ~ MaxPhase로 보정해서 적용. 값이 바뀌면 OnBossPhaseChanged 방송
	// 페이즈가 올라가면 패턴 쿨다운을 초기화하고 자신에게 AI.Event.Boss.PhaseChanged를 보냄 (페이즈 전환 어빌리티 발동)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Boss")
	void SetPhase(int32 NewPhase);

	// AI.Status.Boss.Blocking 보유 여부
	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	bool IsBossBlocking() const;

	// AI.Status.Boss.SuperArmor 보유 여부
	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	bool HasSuperArmor() const;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Boss")
	FOnWarriorBossPhaseChanged OnBossPhaseChanged;

protected:
	//~ Begin APawn Interface.
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss", meta = (AllowPrivateAccess = "true"))
	UBossPatternComponent* BossPatternComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "1"))
	int32 MaxPhase = 2;

	// 페이즈가 올라가는 체력 비율. [0] = 2페이즈 진입, [1] = 3페이즈 진입 ... (큰 값부터)
	// 체력이 줄어들 때만 확인하므로 페이즈는 내려가지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	TArray<float> PhaseHealthThresholds = { 0.5f };

	// 도약 공중 구간에 페이즈가 오르면 착지까지 전환 연출을 미룸. 착지 신호가 오지 않아도 이 시간이 지나면 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0.0", Units = "s"))
	float MaxPhaseTransitionDelay = 3.f;

	// 경직 연속 제한. HitReactWindow 안에 피격 경직(Shared.Ability.HitReact)이 HitReactLimit번 나오면
	// HitReactImmunityDuration 동안 슈퍼아머(AI.Status.SuperArmor)가 되어 경직되지 않음 (약공격 연타로 계속 묶이지 않게)
	// 0이면 제한 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|HitReact", meta = (ClampMin = "0"))
	int32 HitReactLimit = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|HitReact", meta = (ClampMin = "0.0", Units = "s"))
	float HitReactWindow = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|HitReact", meta = (ClampMin = "0.0", Units = "s"))
	float HitReactImmunityDuration = 3.f;

private:
	void HandleCurrentHealthChanged(const FOnAttributeChangeData& Data);

	// 자신에게 AI.Event.Boss.PhaseChanged를 보내 전환 어빌리티 발동
	void TriggerPhaseTransition();

	void HandleAirborneTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 스태거(Shared.Ability.Stagger) 어빌리티 실행 중
	bool IsStaggered() const;

	void HandleAbilityActivated(UGameplayAbility* ActivatedAbility);

	void EndHitReactImmunity();

	// 체력 비율로 계산한 페이즈 (1 ~ MaxPhase)
	int32 ComputePhaseForHealthRatio(float HealthRatio) const;

	UPROPERTY(VisibleInstanceOnly, Category = "Boss")
	int32 CurrentPhase = 1;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AirborneTagChangedHandle;

	bool bPhaseTransitionPending = false;
	FTimerHandle PhaseTransitionDelayTimerHandle;

	// HitReactWindow 안에 경직이 시작된 시간
	TArray<double> RecentHitReactTimes;

	FTimerHandle HitReactImmunityTimerHandle;
	bool bHitReactImmune = false;

public:
	FORCEINLINE UBossPatternComponent* GetBossPatternComponent() const { return BossPatternComponent; }
};
