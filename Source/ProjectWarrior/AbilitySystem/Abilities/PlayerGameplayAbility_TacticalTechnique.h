// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayerGameplayAbility.h"
#include "PlayerGameplayAbility_TacticalTechnique.generated.h"

class UAnimMontage;

/**
 * 전술 모드 전용 기술. 전술 모드(UPlayerGameplayAbility_TacticalMode)가 TriggerAbilityFromGameplayEvent로 발동하고,
 * 이벤트 데이터의 Target에게 순간 대시(모션 워핑)하며 몽타주를 재생한다.
 * 칼 판정은 몽타주의 ANS_ToggleWeaponCollision이 켜고, 맞으면 오는 Shared.Event.MeleeHit으로 데미지를 준다.
 * 스태미나 비용은 Cost + CostGameplayEffectClass(GE_TacticalTechniqueCost)로 낸다. 기술마다 BP 하위 클래스를 만든다.
 */
UCLASS()
class PROJECTWARRIOR_API UPlayerGameplayAbility_TacticalTechnique : public UPlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerGameplayAbility_TacticalTechnique();

	const FText& GetDisplayName() const { return DisplayName; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 커맨드 메뉴에 보이는 이름
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique")
	FText DisplayName;

	// 루트 모션 몽타주. 앞으로 나아가는 루트 모션을 대상까지 늘이거나 줄임
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique")
	TObjectPtr<UAnimMontage> TechniqueMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique", meta = (ClampMin = "0.1"))
	float PlayRate = 1.0f;

	// 워핑 구간(초). End가 0이면 몽타주 끝까지
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Warp", meta = (ClampMin = "0.0"))
	float WarpStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Warp", meta = (ClampMin = "0.0"))
	float WarpEndTime = 0.0f;

	// 대상 캡슐 표면에서 이만큼 앞에 멈춤(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Warp", meta = (ClampMin = "0.0"))
	float StopDistance = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Damage")
	FGameplayTag AttackTypeTag;

	// 맞은 적에게 보내는 히트리액션 이벤트(Shared.Event.HitReact 하위)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Technique|Damage", meta = (Categories = "Shared.Event.HitReact"))
	FGameplayTag HitReactEventTag;

private:
	UFUNCTION()
	void OnMeleeHit(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	static const FName WarpTargetName;
};
