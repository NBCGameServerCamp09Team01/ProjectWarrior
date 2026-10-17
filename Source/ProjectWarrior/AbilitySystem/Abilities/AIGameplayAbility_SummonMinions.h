// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "AIGameplayAbility_SummonMinions.generated.h"

class UAnimMontage;
class UNiagaraSystem;

/**
 * 몽타주를 재생하고, 소환 노티파이(SummonEventTag) 시점에 주변 내비메시 위치로 하수인 폰을 소환하는 어빌리티.
 * 소환된 하수인은 소환자의 공격 대상을 초기 대상으로 받는다. 값만 설정해서 사용
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_SummonMinions : public UAIGameplayAbility
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_SummonMinions();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	//~ End GameplayAbility Interface

	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	TObjectPtr<UAnimMontage> SummonMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	TSubclassOf<APawn> MinionClass;

	// 몽타주에서 이 태그를 보내는 노티파이 시점에 소환. 노티파이가 없으면 몽타주가 끝날 때 소환
	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	FGameplayTag SummonEventTag;

	// 한 번에 소환할 수
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "1"))
	int32 SummonCount = 3;

	// 이 어빌리티로 소환해서 살아 있는 하수인의 최대 수. 이미 다 차 있으면 발동하지 않음 (0 = 제한 없음)
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "0"))
	int32 MaxAliveMinions = 6;

	// 소환 위치를 찾을 반경 (소환자 중심)
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonRadius = 500.f;

	// 소환자 몸과 겹치지 않도록 둘 최소 거리
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSummonDistance = 250.f;

	// 내비메시 위치에서 위로 띄울 높이 (하수인 캡슐 반높이 이상)
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (Units = "cm"))
	float SpawnHeightOffset = 80.f;

	// 각 소환 위치에 재생할 이펙트 (선택)
	UPROPERTY(EditDefaultsOnly, Category = "Summon|FX")
	TObjectPtr<UNiagaraSystem> SummonEffect;

	// 소환된 하수인이 처음에 재생할 등장 몽타주 (땅에서 파고 나오기 등, 선택)
	// 재생하는 동안 하수인의 이동과 AI 로직을 멈췄다가 끝나면 재개
	UPROPERTY(EditDefaultsOnly, Category = "Summon|Emerge")
	TObjectPtr<UAnimMontage> MinionEmergeMontage;

	// 등장 몽타주 대신 이 시간 동안만 멈춤 (0 이하면 몽타주 길이 사용)
	UPROPERTY(EditDefaultsOnly, Category = "Summon|Emerge", meta = (Units = "s"))
	float MinionEmergeDurationOverride = 0.f;

private:
	UFUNCTION()
	void HandleSummonEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageEnded();

	UFUNCTION()
	void HandleMontageInterrupted();

	void SummonMinions();

	int32 GetAliveMinionCount();

	bool FindSummonLocation(const FVector& Origin, FVector& OutLocation) const;

	// 하수인에게 등장 몽타주를 재생시키고 그동안 이동·AI 로직을 멈춤
	void PlayMinionEmerge(APawn* Minion) const;

	// 소환자가 죽으면 살아 있는 하수인도 함께 죽임 (웨이브는 소환자만 세므로 하수인이 남지 않도록)
	void BindSummonerDeath();
	void HandleSummonerDeathTagChanged(const FGameplayTag Tag, int32 NewCount);
	void KillSummonedMinions();

	FDelegateHandle SummonerDeathHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundSummonerASC;

	// 이번 발동에서 이미 소환했는지 (노티파이가 없으면 몽타주 종료 시 소환)
	bool bSummonedThisActivation = false;

	TArray<TWeakObjectPtr<APawn>> SummonedMinions;
};
