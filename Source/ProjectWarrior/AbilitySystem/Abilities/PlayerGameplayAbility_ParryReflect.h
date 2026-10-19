// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayerGameplayAbility.h"
#include "PlayerGameplayAbility_ParryReflect.generated.h"

/**
 * 퍼펙트 패링(Player.Status.Blocking.Perfect)으로 막은 투사체를 발사자에게 되돌리는 패시브 어빌리티.
 * 막기 이벤트(Player.Event.Successful.Block)로 발동하며, 투사체는 이벤트의 OptionalObject로 넘어온다.
 * 이 어빌리티를 부여하지 않거나 ActivationRequiredTags(스킬 해금 태그 등)를 만족하지 않으면 막힌 투사체는 그대로 사라진다.
 */
UCLASS()
class PROJECTWARRIOR_API UPlayerGameplayAbility_ParryReflect : public UPlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerGameplayAbility_ParryReflect();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~ End GameplayAbility Interface

	// 되돌린 투사체의 기본 피해 배율. 피해는 플레이어의 공격력으로 다시 계산됨
	UPROPERTY(EditDefaultsOnly, Category = "ParryReflect", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;

	// 되돌린 투사체의 속도 배율
	UPROPERTY(EditDefaultsOnly, Category = "ParryReflect", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.5f;
};
