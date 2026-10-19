// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FGameplayEffectModCallbackData;

/**
 * 수식어 피격 효과(화염 화상, 냉기 둔화, 흡혈). 맞는 쪽(UWarriorAttributeSet::PostGameplayEffectExecute의 DamageTaken)에서 한 번 부른다.
 * 근접(NativeApplyEffectSpecHandleToTarget)과 투사체(ApplyGameplayEffectSpecToSelf)가 모두 이 한 곳을 지난다.
 * 대상이 플레이어이고 instigator가 수식어를 가진 살아 있는 AWarriorAICharacter일 때만 처리한다
 * (AI끼리, 반사한 투사체 = instigator가 플레이어는 빠짐). 수식어가 만든 GE(Shared.Effect.Affix)는 다시 처리하지 않는다.
 */
namespace WarriorAffix
{
	void HandleDamageTaken(const FGameplayEffectModCallbackData& Data, float InDamageDone);
}
