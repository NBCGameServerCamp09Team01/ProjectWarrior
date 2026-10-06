// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_Dodge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

UAIGameplayAbility_Dodge::UAIGameplayAbility_Dodge()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer DodgeAssetTags;
	DodgeAssetTags.AddTag(WarriorGameplayTags::AI_Ability_Dodge);
	SetAssetTags(DodgeAssetTags);

	ActivationOwnedTags.AddTag(WarriorGameplayTags::AI_Status_Dodging);

	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Finisher);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::AI_Status_Guarding);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = WarriorGameplayTags::AI_Event_IncomingAttack;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

bool UAIGameplayAbility_Dodge::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload) || !Payload || !Payload->Instigator || !ActorInfo)
	{
		return false;
	}

	const UWorld* World = ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
	if (!World || World->GetTimeSeconds() - LastDodgeTime < DodgeCooldown)
	{
		return false;
	}

	// 공격 동작, 피격 경직·스태거·처형·사망 중에는 회피하지 않음
	if (IsAttacking(ActorInfo->AbilitySystemComponent.Get()) || IsOwnerIncapacitated(ActorInfo->AbilitySystemComponent.Get()))
	{
		return false;
	}

	return FMath::FRand() < DodgeChance;
}

void UAIGameplayAbility_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !TriggerEventData || !TriggerEventData->Instigator)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Attacker = TriggerEventData->Instigator.Get();
	LastDodgeTime = GetWorld()->GetTimeSeconds();

	// 반응 지연 중에 처형이 시작되면 처형 몽타주를 덮어쓰지 않도록 취소
	WatchForFinisherOrDeath();

	if (const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AAIController* AIController = Cast<AAIController>(AvatarPawn->GetController()))
		{
			AIController->StopMovement();
		}
	}

	const float Delay = FMath::FRandRange(FMath::Min(ReactionDelay.X, ReactionDelay.Y), FMath::Max(ReactionDelay.X, ReactionDelay.Y));
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Delay);
	DelayTask->OnFinish.AddDynamic(this, &ThisClass::OnReactionDelayFinished);
	DelayTask->ReadyForActivation();
}

void UAIGameplayAbility_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetInvulnerable(false);
	Attacker.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_Dodge::OnReactionDelayFinished()
{
	// 지연 동안 처형·경직 등이 시작됐으면 회피하지 않음
	if (IsOwnerIncapacitated(GetAbilitySystemComponentFromActorInfo()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	FVector DodgeDirection;
	UAnimMontage* Montage = nullptr;

	if (Avatar && Attacker.IsValid() && ChooseDodgeDirection(Avatar, Attacker.Get(), DodgeDirection))
	{
		Montage = FindMontageForWorldDirection(Avatar, DodgeDirection);
	}

	if (!Montage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	SetInvulnerable(true);

	UAbilityTask_WaitDelay* InvulnerableTask = UAbilityTask_WaitDelay::WaitDelay(this, InvulnerableDuration);
	InvulnerableTask->OnFinish.AddDynamic(this, &ThisClass::OnInvulnerabilityFinished);
	InvulnerableTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnDodgeMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnDodgeMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnDodgeMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnDodgeMontageFinished);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_Dodge::OnInvulnerabilityFinished()
{
	SetInvulnerable(false);
}

void UAIGameplayAbility_Dodge::OnDodgeMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UAIGameplayAbility_Dodge::ChooseDodgeDirection(const AActor* Avatar, const AActor* InAttacker, FVector& OutWorldDirection) const
{
	const FVector Location = Avatar->GetActorLocation();

	FVector Away = (Location - InAttacker->GetActorLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector Side(-Away.Y, Away.X, 0.f);

	struct FCandidate { FVector Direction; float Weight; };
	const FCandidate Candidates[] = { { Away, BackwardWeight }, { Side, SideWeight }, { -Side, SideWeight } };

	TArray<FCandidate, TInlineAllocator<3>> Open;
	float TotalWeight = 0.f;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Candidate.Weight > 0.f && HasClearance(Location, Candidate.Direction))
		{
			Open.Add(Candidate);
			TotalWeight += Candidate.Weight;
		}
	}

	if (Open.IsEmpty())
	{
		return false;
	}

	float Pick = FMath::FRand() * TotalWeight;
	for (const FCandidate& Candidate : Open)
	{
		Pick -= Candidate.Weight;
		if (Pick <= 0.f)
		{
			OutWorldDirection = Candidate.Direction;
			return true;
		}
	}

	OutWorldDirection = Open.Last().Direction;
	return true;
}

bool UAIGameplayAbility_Dodge::HasClearance(const FVector& Start, const FVector& Direction) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem || ClearanceDistance <= 0.f)
	{
		return true;
	}

	const FVector Extent(50.f, 50.f, 250.f);
	FNavLocation ProjectedStart;
	if (!NavSystem->ProjectPointToNavigation(Start, ProjectedStart, Extent))
	{
		return true;
	}

	FVector HitLocation;
	return !UNavigationSystemV1::NavigationRaycast(GetWorld(), ProjectedStart.Location, ProjectedStart.Location + Direction * ClearanceDistance, HitLocation);
}

UAnimMontage* UAIGameplayAbility_Dodge::FindMontageForWorldDirection(const AActor* Avatar, const FVector& WorldDirection) const
{
	// 몽타주 방향은 AI가 바라보는 방향 기준
	const FVector Local = Avatar->GetActorTransform().InverseTransformVectorNoScale(WorldDirection);

	EWarriorDodgeDirection Direction;
	if (FMath::Abs(Local.X) >= FMath::Abs(Local.Y))
	{
		Direction = Local.X >= 0.f ? EWarriorDodgeDirection::Forward : EWarriorDodgeDirection::Backward;
	}
	else
	{
		Direction = Local.Y >= 0.f ? EWarriorDodgeDirection::Right : EWarriorDodgeDirection::Left;
	}

	if (const TObjectPtr<UAnimMontage>* Montage = DodgeMontages.Find(Direction); Montage && *Montage)
	{
		return *Montage;
	}

	const TObjectPtr<UAnimMontage>* Fallback = DodgeMontages.Find(EWarriorDodgeDirection::Backward);
	return Fallback ? Fallback->Get() : nullptr;
}

bool UAIGameplayAbility_Dodge::IsAttacking(const UAbilitySystemComponent* ASC) const
{
	if (!ASC)
	{
		return false;
	}

	static const FGameplayTagContainer AttackAbilityTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		WarriorGameplayTags::AI_Ability_Melee, WarriorGameplayTags::AI_Ability_Range, WarriorGameplayTags::AI_Ability_Boss });

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && Spec.Ability->GetAssetTags().HasAny(AttackAbilityTags))
		{
			return true;
		}
	}

	return false;
}

void UAIGameplayAbility_Dodge::SetInvulnerable(bool bEnable)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bInvulnerable == bEnable)
	{
		return;
	}

	bInvulnerable = bEnable;

	if (bEnable)
	{
		ASC->AddLooseGameplayTag(WarriorGameplayTags::Shared_Status_Dodge);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(WarriorGameplayTags::Shared_Status_Dodge);
	}
}
