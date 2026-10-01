// Fill out your copyright notice in the Description page of Project Settings.


#include "BossPatternComponent.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Characters/WarriorBossCharacter.h"
#include "ProjectWarrior/DataAssets/DataAsset_BossPatternSet.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/CapsuleComponent.h"

bool UBossPatternComponent::SelectPattern(AActor* TargetActor, FGameplayTag& OutPatternTag)
{
	OutPatternTag = FGameplayTag();

	float Distance = 0.f;
	float Angle = 0.f;

	if (!PatternSet || !GetTargetDistanceAndAngle(TargetActor, Distance, Angle))
	{
		return false;
	}

	const int32 CurrentPhase = GetCurrentPhase();

	TArray<const FWarriorBossPatternData*> Candidates;
	float TotalWeight = 0.f;

	for (const FWarriorBossPatternData& Pattern : PatternSet->GetPatterns())
	{
		if (!Pattern.AbilityTag.IsValid() || Pattern.Weight <= 0.f) continue;
		if (!Pattern.IsUsableInPhase(CurrentPhase)) continue;
		if (Distance < Pattern.MinRange || Distance > Pattern.MaxRange) continue;
		if (Angle < Pattern.MinAngle || Angle > Pattern.MaxAngle) continue;
		if (!Pattern.bAllowRepeat && Pattern.AbilityTag == LastPatternTag) continue;
		if (IsPatternOnCooldown(Pattern.AbilityTag)) continue;
		if (!CanActivatePatternAbility(Pattern.AbilityTag)) continue;

		Candidates.Add(&Pattern);
		TotalWeight += Pattern.Weight;
	}

	if (Candidates.IsEmpty())
	{
		if (bLogPatternSelection)
		{
			UE_LOG(LogProjectWarrior, Log, TEXT("[BossPattern] No candidate. Distance %.0f, Angle %.0f, Phase %d"), Distance, Angle, CurrentPhase);
		}

		return false;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	const FWarriorBossPatternData* SelectedPattern = Candidates.Last();

	for (const FWarriorBossPatternData* Candidate : Candidates)
	{
		Roll -= Candidate->Weight;

		if (Roll <= 0.f)
		{
			SelectedPattern = Candidate;
			break;
		}
	}

	OutPatternTag = SelectedPattern->AbilityTag;

	if (bLogPatternSelection)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[BossPattern] Selected %s (%d candidates). Distance %.0f, Angle %.0f, Phase %d"),
			*OutPatternTag.ToString(), Candidates.Num(), Distance, Angle, CurrentPhase);
	}

	return true;
}

void UBossPatternComponent::NotifyPatternActivated(FGameplayTag PatternTag)
{
	if (!PatternTag.IsValid())
	{
		return;
	}

	LastPatternTag = PatternTag;

	const FWarriorBossPatternData* Pattern = PatternSet ? PatternSet->FindPattern(PatternTag) : nullptr;

	if (Pattern && Pattern->Cooldown > 0.f)
	{
		PatternCooldownEndTimes.Add(PatternTag, GetWorld()->GetTimeSeconds() + Pattern->Cooldown);
	}
}

bool UBossPatternComponent::IsPatternOnCooldown(FGameplayTag PatternTag) const
{
	return GetPatternRemainingCooldown(PatternTag) > 0.f;
}

float UBossPatternComponent::GetPatternRemainingCooldown(FGameplayTag PatternTag) const
{
	const double* EndTime = PatternCooldownEndTimes.Find(PatternTag);

	if (!EndTime)
	{
		return 0.f;
	}

	return FMath::Max(0.f, static_cast<float>(*EndTime - GetWorld()->GetTimeSeconds()));
}

void UBossPatternComponent::ResetAllPatternCooldowns()
{
	PatternCooldownEndTimes.Reset();
	LastPatternTag = FGameplayTag();
}

bool UBossPatternComponent::GetTargetDistanceAndAngle(AActor* TargetActor, float& OutDistance, float& OutAngle) const
{
	OutDistance = 0.f;
	OutAngle = 0.f;

	const APawn* OwningPawn = GetOwningPawn();

	if (!TargetActor || !OwningPawn)
	{
		return false;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - OwningPawn->GetActorLocation();

	OutDistance = ToTarget.Size2D();

	if (bUseCapsuleSurfaceDistance)
	{
		const ACharacter* OwningCharacter = Cast<ACharacter>(OwningPawn);
		const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);

		const float OwnerRadius = OwningCharacter ? OwningCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
		const float TargetRadius = TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;

		OutDistance = FMath::Max(0.f, OutDistance - OwnerRadius - TargetRadius);
	}

	const FVector ToTargetDirection = ToTarget.GetSafeNormal2D();

	if (!ToTargetDirection.IsNearlyZero())
	{
		const float DotResult = FVector::DotProduct(OwningPawn->GetActorForwardVector().GetSafeNormal2D(), ToTargetDirection);
		OutAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotResult, -1.f, 1.f)));
	}

	return true;
}

bool UBossPatternComponent::CanActivatePatternAbility(const FGameplayTag& PatternTag) const
{
	const AWarriorBaseCharacter* OwningCharacter = Cast<AWarriorBaseCharacter>(GetOwner());
	UWarriorAbilitySystemComponent* ASC = OwningCharacter ? OwningCharacter->GetWarriorAbilitySystemComponent() : nullptr;

	if (!ASC)
	{
		return false;
	}

	TArray<FGameplayAbilitySpec*> FoundAbilitySpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(PatternTag.GetSingleTagContainer(), FoundAbilitySpecs);

	for (const FGameplayAbilitySpec* AbilitySpec : FoundAbilitySpecs)
	{
		// 태그 요구조건 외에 어빌리티 자체 쿨다운·코스트까지 확인
		if (AbilitySpec && AbilitySpec->Ability && !AbilitySpec->IsActive()
			&& AbilitySpec->Ability->CanActivateAbility(AbilitySpec->Handle, ASC->AbilityActorInfo.Get()))
		{
			return true;
		}
	}

	return false;
}

int32 UBossPatternComponent::GetCurrentPhase() const
{
	const AWarriorBossCharacter* BossCharacter = Cast<AWarriorBossCharacter>(GetOwner());

	return BossCharacter ? BossCharacter->GetCurrentPhase() : 1;
}
