// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility_BossAreaAttack.h"
#include "ScalableFloat.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "AIGameplayAbility_BurrowAmbush.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;
class UNiagaraComponent;
class UEnvQuery;

UENUM(BlueprintType)
enum class EWarriorAmbushPlacement : uint8
{
	// 대상이 바라보는 방향의 반대쪽 (사각)
	BehindTarget,
	// 대상의 왼쪽 또는 오른쪽 (무작위)
	BesideTarget,
	// 대상 주변 무작위 방향
	AroundTarget
};

/**
 * 땅속으로 파고들었다가 대상 근처(기본: 등 뒤)로 이동해 튀어나오며 범위 판정하는 공격.
 * 흐름: 파고들기 몽타주(BurrowMontage, 역재생 가능) -> 숨김·충돌 해제 상태로 UndergroundDuration 동안 대기
 *       (이때 튀어나올 위치에 DefaultAreaData 위험 범위 표시) -> 순간이동 -> 등장 몽타주 + EmergeImpactDelay 후 판정
 * 튀어나올 위치는 파고드는 시점에 확정되므로, 표시를 보고 피할 수 있다. 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BurrowAmbush : public UAIGameplayAbility_BossAreaAttack
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BurrowAmbush();

protected:
	//~ Begin GameplayAbility Interface.
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 파고들기 몽타주 (선택). bPlayBurrowReversed면 끝에서 처음으로 역재생 (등장 애니메이션 재사용)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow")
	TObjectPtr<UAnimMontage> BurrowMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Burrow")
	bool bPlayBurrowReversed = true;

	UPROPERTY(EditDefaultsOnly, Category = "Burrow", meta = (ClampMin = "0.1"))
	float BurrowPlayRate = 1.5f;

	// 파고드는 동안 메시를 이 깊이만큼 부드럽게 내려 땅속으로 들어가는 것처럼 보이게 함 (0 = 사용 안 함)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow", meta = (ClampMin = "0.0", Units = "cm"))
	float BurrowSinkDepth = 0.f;

	// 파고들기 시간 중 가라앉기 시작하는 지점 (0 = 처음부터, 0.5 = 절반부터)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float BurrowSinkStartFraction = 0.2f;

	// 가라앉는 가속 정도. 클수록 처음엔 거의 그대로 있다가 끝에서 빠르게 들어감 (1 = 일정한 속도)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float BurrowSinkEaseExponent = 3.f;

	// 땅속에 머무는 시간. 튀어나올 위치의 위험 범위가 이 시간 + EmergeImpactDelay 동안 표시됨
	UPROPERTY(EditDefaultsOnly, Category = "Burrow", meta = (ClampMin = "0.0", Units = "s"))
	float UndergroundDuration = 0.8f;

	// 튀어나올 위치를 찾는 EQS (대상 주변 원형 아이템 생성 -> 필터·점수). 비어 있거나 실패하면 Placement 방식으로 계산
	// 파고드는 동안 비동기로 실행하고, 땅속으로 들어가는 시점의 결과로 위치를 확정
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Ambush")
	TObjectPtr<UEnvQuery> AmbushQuery;

	// 결과(점수순) 상위 이 비율 안에서 무작위로 고름. 다른 개체가 예약한 위치 근처는 건너뜀
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Ambush", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbushPickTopFraction = 0.25f;

	// 다른 개체가 예약한 등장 위치와 이 거리 이내면 그 후보는 쓰지 않음 (같은 자리에 겹쳐 나오지 않도록)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Ambush", meta = (ClampMin = "0.0", Units = "cm"))
	float MinEmergeSeparation = 150.f;

	// 동시에 땅을 파고 있을 수 있는 개체 수 (BurrowingStatusTag를 가진 폰 수로 셈, 0 = 제한 없음)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Limit", meta = (ClampMin = "0"))
	int32 MaxConcurrentBurrowers = 3;

	// 땅을 파는 동안 붙는 상태 태그. 비어 있으면 개체 수 제한을 쓰지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Limit")
	FGameplayTag BurrowingStatusTag;

	// EQS를 쓰지 않거나 실패했을 때의 배치 방식
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Ambush")
	EWarriorAmbushPlacement Placement = EWarriorAmbushPlacement::BehindTarget;

	// 대상 중심에서 튀어나올 위치까지의 거리
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Ambush", meta = (ClampMin = "0.0", Units = "cm"))
	float AmbushDistance = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Ambush")
	FVector NavProjectExtent = FVector(150.f, 150.f, 300.f);

	// 등장 몽타주 (땅에서 파고 나오기)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Emerge")
	TObjectPtr<UAnimMontage> EmergeMontage;

	// 등장 몽타주 시작부터 판정까지의 시간
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Emerge", meta = (ClampMin = "0.0", Units = "s"))
	float EmergeImpactDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Burrow|Damage")
	FScalableFloat DamageScalableFloat;

	// 파고드는 위치와 튀어나오는 위치에 재생할 이펙트 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|FX")
	TObjectPtr<UNiagaraSystem> BurrowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Burrow|FX")
	TObjectPtr<UNiagaraSystem> EmergeEffect;

	// 땅속에 있는 동안 튀어나올 위치에서 반복 재생할 예고 이펙트 (흙 튀김 등, 루프 시스템). 튀어나오는 순간 꺼짐
	UPROPERTY(EditDefaultsOnly, Category = "Burrow|FX")
	TObjectPtr<UNiagaraSystem> EmergeWarningEffect;

private:
	UFUNCTION()
	void HandleBurrowed();

	UFUNCTION()
	void HandleEmerge();

	UFUNCTION()
	void HandleEmergeImpact();

	UFUNCTION()
	void HandleEmergeMontageEnded();

	UFUNCTION()
	void HandleEmergeMontageInterrupted();

	// 대상 기준으로 튀어나올 발 위치를 계산 (내비메시 보정). 대상이 없으면 false
	bool ComputeEmergeLocation(FVector& OutFeetLocation, FRotator& OutFacing);

	void SetUnderground(bool bUnderground);

	void HandleAmbushQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	void AbortAmbushQuery();

	// 대상을 향하는 방향 (대상이 없으면 현재 방향)
	FRotator ComputeFacingToTarget(const FVector& FromLocation) const;

	// 월드에서 BurrowingStatusTag를 가진 폰 수 (자신 제외)
	int32 CountOtherBurrowers(const AActor* Self) const;

	void SetBurrowingTag(bool bEnable);

	// 다른 개체가 예약한 등장 위치와 MinEmergeSeparation 이내인지
	bool IsNearReservedLocation(const FVector& Location) const;

	void ReserveEmergeLocation();

	void ReleaseEmergeLocation();

	void StopEmergeWarningEffect();

	void StartBurrowSink(float BurrowDuration);

	void TickBurrowSink();

	// 메시 높이를 원래대로 되돌림
	void StopBurrowSink();

	FTimerHandle BurrowSinkTimerHandle;

	float BurrowSinkStartTime = 0.f;
	float BurrowSinkDuration = 0.f;

	// 가라앉기 전 메시의 상대 Z
	float MeshBaseRelativeZ = 0.f;

	bool bMeshSunk = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> EmergeWarningComponent;

	bool bHasReservedLocation = false;

	FVector EmergeFeetLocation = FVector::ZeroVector;
	FRotator EmergeFacing = FRotator::ZeroRotator;

	bool bIsUnderground = false;

	// 땅속으로 들어간 뒤에는 EQS 결과가 늦게 와도 위치를 바꾸지 않음 (표시된 범위 = 등장 위치)
	bool bEmergeLocationLocked = false;

	bool bHasBurrowingTag = false;

	int32 AmbushQueryRequestId = INDEX_NONE;
};
