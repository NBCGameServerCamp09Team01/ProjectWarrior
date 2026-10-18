// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAffixLibrary.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/Stage/WarriorAffixCatalogDataAsset.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/ProjectWarrior.h"

namespace WarriorAffix
{
	void HandleDamageTaken(const FGameplayEffectModCallbackData& Data, float InDamageDone)
	{
		if (InDamageDone <= 0.f || Data.EffectSpec.GetDynamicAssetTags().HasTag(WarriorGameplayTags::Shared_Effect_Affix))
		{
			return;
		}

		AWarriorAICharacter* Attacker = Cast<AWarriorAICharacter>(Data.EffectSpec.GetContext().GetInstigator());
		AWarriorPlayerCharacter* Victim = Cast<AWarriorPlayerCharacter>(Data.Target.GetAvatarActor());
		if (!Attacker || !Victim || Attacker->GetAffixIds().IsEmpty() || UWarriorFunctionLibrary::IsActorDead(Attacker))
		{
			return;
		}

		const UWarriorAffixCatalogDataAsset* Catalog = UWarriorAffixCatalogDataAsset::LoadFromSettings();
		UAbilitySystemComponent* AttackerASC = Attacker->GetAbilitySystemComponent();
		UAbilitySystemComponent* VictimASC = &Data.Target;
		if (!Catalog || !AttackerASC)
		{
			return;
		}

		for (const FName AffixId : Attacker->GetAffixIds())
		{
			const FWarriorAffixDefinition* Definition = Catalog->FindAffix(AffixId);
			if (!Definition)
			{
				continue;
			}

			// 플레이어에게 불리한 효과가 실제로 걸린 시간(화상·둔화 중 긴 쪽). 0이면 플레이어 몸 이펙트 없음(흡혈 등)
			float DebuffSeconds = 0.f;

			// 화상 등: 틱당 피해 = 맞은 피해 × 비율. 스택·시간은 GE가 정한다(지속 시간만 새로 고침)
			if (Definition->OnHitEffect)
			{
				FGameplayEffectContextHandle Context = AttackerASC->MakeEffectContext();
				Context.AddInstigator(Attacker, Attacker);
				FGameplayEffectSpecHandle Spec = AttackerASC->MakeOutgoingSpec(Definition->OnHitEffect, 1.f, Context);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(WarriorGameplayTags::Shared_SetByCaller_AffixDamage, InDamageDone * Definition->OnHitDamageRatio);
					Spec.Data->AddDynamicAssetTag(WarriorGameplayTags::Shared_Effect_Affix);
					const FActiveGameplayEffectHandle Applied = AttackerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, VictimASC);
					if (Applied.WasSuccessfullyApplied())
					{
						DebuffSeconds = FMath::Max(DebuffSeconds, Spec.Data->GetDuration());
					}
				}
			}

			// 둔화: 최종 이동 속도에 배율. 같은 수식어에 다시 맞으면 시간만 새로 고침
			if (Definition->OnHitMoveSpeedScale < 1.f && Definition->OnHitSlowSeconds > 0.f)
			{
				Victim->ApplyTimedMovementSpeedScale(AffixId, Definition->OnHitMoveSpeedScale, Definition->OnHitSlowSeconds);
				DebuffSeconds = FMath::Max(DebuffSeconds, Definition->OnHitSlowSeconds);
			}

			// 불리한 효과가 걸린 동안만 그 수식어 색 오라를 플레이어 몸에(다시 맞으면 시간만 새로 고침)
			if (DebuffSeconds > 0.f)
			{
				Victim->ShowTimedStatusEffect(AffixId, Catalog->GetAuraSystem(), Definition->AuraColor * Catalog->GetVictimEffectIntensity(), DebuffSeconds);
			}

			// 흡혈: 준 피해 × 비율을 적 체력으로. CurrentHealth 쪽 처리(잘라내기·체력바)를 그대로 탄다
			if (Definition->LifeStealRatio > 0.f)
			{
				AttackerASC->ApplyModToAttribute(UWarriorAttributeSet::GetCurrentHealthAttribute(), EGameplayModOp::AddBase,
					InDamageDone * Definition->LifeStealRatio);
			}
		}

		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Affix] %s hit %s for %.1f with %d affix(es)"),
			*Attacker->GetName(), *Victim->GetName(), InDamageDone, Attacker->GetAffixIds().Num());
	}
}
