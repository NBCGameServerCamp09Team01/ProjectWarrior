// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorBossCharacter.h"
#include "ProjectWarrior/Components/Combat/BossPatternComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

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

	OnBossPhaseChanged.Broadcast(OldPhase, CurrentPhase);
}

bool AWarriorBossCharacter::IsBossBlocking() const
{
	return WarriorAbilitySystemComponent && WarriorAbilitySystemComponent->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_Boss_Blocking);
}

bool AWarriorBossCharacter::HasSuperArmor() const
{
	return WarriorAbilitySystemComponent && WarriorAbilitySystemComponent->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_Boss_SuperArmor);
}
