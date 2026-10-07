// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorBossCharacter.h"
#include "ProjectWarrior/Components/Combat/BossPatternComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "TimerManager.h"

AWarriorBossCharacter::AWarriorBossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BossPatternComponent = CreateDefaultSubobject<UBossPatternComponent>("BossPatternComponent");
}

void AWarriorBossCharacter::SetPhase(int32 NewPhase)
{
	NewPhase = FMath::Clamp(NewPhase, 1, MaxPhase);

	if (NewPhase == CurrentPhase)
	{
		return;
	}

	const int32 OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	if (CurrentPhase > OldPhase)
	{
		// 새 페이즈 패턴이 1페이즈 쿨다운에 묶이지 않도록
		if (BossPatternComponent)
		{
			BossPatternComponent->ResetAllPatternCooldowns();
		}

		// 도약 공중 구간이면 착지할 때까지 연출을 미룸 (페이즈 값과 패턴 목록은 바로 바뀜)
		if (WarriorAbilitySystemComponent && WarriorAbilitySystemComponent->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_Boss_Airborne))
		{
			bPhaseTransitionPending = true;

			GetWorldTimerManager().SetTimer(PhaseTransitionDelayTimerHandle, this, &ThisClass::TriggerPhaseTransition, FMath::Max(MaxPhaseTransitionDelay, 0.01f), false);
		}
		else
		{
			TriggerPhaseTransition();
		}
	}

	OnBossPhaseChanged.Broadcast(OldPhase, CurrentPhase);
}

void AWarriorBossCharacter::TriggerPhaseTransition()
{
	bPhaseTransitionPending = false;
	GetWorldTimerManager().ClearTimer(PhaseTransitionDelayTimerHandle);

	// 미루는 사이에 죽었으면 연출 없음
	if (!WarriorAbilitySystemComponent || UWarriorFunctionLibrary::IsActorDead(this))
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.Instigator = this;
	EventData.Target = this;
	EventData.EventMagnitude = CurrentPhase;

	// 전환 어빌리티가 스태거를 끊기 전에 알려 줌. 그로기 중이면 충격파를 내지 않음 (반격 중인 플레이어가 맞지 않게)
	if (IsStaggered())
	{
		EventData.InstigatorTags.AddTag(WarriorGameplayTags::Shared_Ability_Stagger);
	}

	if (WarriorAbilitySystemComponent->HandleGameplayEvent(WarriorGameplayTags::AI_Event_Boss_PhaseChanged, &EventData) == 0)
	{
		// 진행 중인 패턴이 전환 어빌리티를 막고 있으면 끊고 한 번 더 시도
		const FGameplayTagContainer BossAbilityTags(WarriorGameplayTags::AI_Ability_Boss);
		WarriorAbilitySystemComponent->CancelAbilities(&BossAbilityTags);
		WarriorAbilitySystemComponent->HandleGameplayEvent(WarriorGameplayTags::AI_Event_Boss_PhaseChanged, &EventData);
	}
}

void AWarriorBossCharacter::HandleAirborneTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0 && bPhaseTransitionPending)
	{
		TriggerPhaseTransition();
	}
}

bool AWarriorBossCharacter::IsStaggered() const
{
	if (!WarriorAbilitySystemComponent)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : WarriorAbilitySystemComponent->GetActivatableAbilities())
	{
		if (AbilitySpec.IsActive() && AbilitySpec.Ability && AbilitySpec.Ability->GetAssetTags().HasTag(WarriorGameplayTags::Shared_Ability_Stagger))
		{
			return true;
		}
	}

	return false;
}

void AWarriorBossCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (WarriorAbilitySystemComponent && !AirborneTagChangedHandle.IsValid())
	{
		AirborneTagChangedHandle = WarriorAbilitySystemComponent->RegisterGameplayTagEvent(WarriorGameplayTags::AI_Status_Boss_Airborne, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleAirborneTagChanged);
	}

	if (WarriorAbilitySystemComponent && !HealthChangedHandle.IsValid())
	{
		HealthChangedHandle = WarriorAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UWarriorAttributeSet::GetCurrentHealthAttribute())
			.AddUObject(this, &ThisClass::HandleCurrentHealthChanged);
	}

	if (WarriorAbilitySystemComponent && !AbilityActivatedHandle.IsValid())
	{
		AbilityActivatedHandle = WarriorAbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
	}
}

void AWarriorBossCharacter::HandleAbilityActivated(UGameplayAbility* ActivatedAbility)
{
	if (HitReactLimit <= 0 || bHitReactImmune || !ActivatedAbility
		|| !ActivatedAbility->GetAssetTags().HasTag(WarriorGameplayTags::Shared_Ability_HitReact))
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	RecentHitReactTimes.RemoveAll([Now, this](double HitReactTime)
		{
			return Now - HitReactTime > HitReactWindow;
		});
	RecentHitReactTimes.Add(Now);

	if (RecentHitReactTimes.Num() < HitReactLimit)
	{
		return;
	}

	// 이번 경직은 그대로 재생하고, 그 뒤로는 경직 없이 행동 (다음 패턴으로 반격)
	RecentHitReactTimes.Reset();
	bHitReactImmune = true;

	WarriorAbilitySystemComponent->AddLooseGameplayTag(WarriorGameplayTags::AI_Status_SuperArmor);

	GetWorldTimerManager().SetTimer(HitReactImmunityTimerHandle, this, &ThisClass::EndHitReactImmunity, FMath::Max(HitReactImmunityDuration, 0.01f), false);
}

void AWarriorBossCharacter::EndHitReactImmunity()
{
	if (!bHitReactImmune)
	{
		return;
	}

	bHitReactImmune = false;

	if (WarriorAbilitySystemComponent)
	{
		WarriorAbilitySystemComponent->RemoveLooseGameplayTag(WarriorGameplayTags::AI_Status_SuperArmor);
	}
}

void AWarriorBossCharacter::HandleCurrentHealthChanged(const FOnAttributeChangeData& Data)
{
	// 시작 데이터가 체력을 채울 때(증가)는 무시. 0이면 사망 처리가 우선
	if (Data.NewValue >= Data.OldValue || Data.NewValue <= 0.f || !WarriorAttributeSet)
	{
		return;
	}

	const float MaxHealth = WarriorAttributeSet->GetMaxHealth();

	if (MaxHealth <= 0.f)
	{
		return;
	}

	const int32 HealthPhase = ComputePhaseForHealthRatio(Data.NewValue / MaxHealth);

	if (HealthPhase > CurrentPhase)
	{
		SetPhase(HealthPhase);
	}
}

int32 AWarriorBossCharacter::ComputePhaseForHealthRatio(float HealthRatio) const
{
	int32 Phase = 1;

	for (const float Threshold : PhaseHealthThresholds)
	{
		if (HealthRatio <= Threshold)
		{
			++Phase;
		}
	}

	return FMath::Clamp(Phase, 1, MaxPhase);
}

bool AWarriorBossCharacter::IsBossBlocking() const
{
	return WarriorAbilitySystemComponent && WarriorAbilitySystemComponent->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_Boss_Blocking);
}

bool AWarriorBossCharacter::HasSuperArmor() const
{
	return WarriorAbilitySystemComponent && WarriorAbilitySystemComponent->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_Boss_SuperArmor);
}
