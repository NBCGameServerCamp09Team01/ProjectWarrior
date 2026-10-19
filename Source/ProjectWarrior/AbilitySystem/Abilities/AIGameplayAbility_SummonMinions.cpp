// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_SummonMinions.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Controllers/WarriorAIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UAIGameplayAbility_SummonMinions::UAIGameplayAbility_SummonMinions()
{
	// 소환한 하수인 목록을 발동 사이에 유지해야 하므로 액터당 인스턴스
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UAIGameplayAbility_SummonMinions::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 부모가 토큰 부족 등으로 이미 끝냈으면 진행하지 않음
	if (!IsActive())
	{
		return;
	}

	if (!SummonMontage || !MinionClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[SummonMinions] %s needs SummonMontage and MinionClass."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 하수인이 이미 다 차 있으면 몽타주를 재생하지 않고 실패 (BT가 다른 패턴을 고르도록)
	if (MaxAliveMinions > 0 && GetAliveMinionCount() >= MaxAliveMinions)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bSummonedThisActivation = false;

	if (SummonEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* SummonTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, SummonEventTag, nullptr, true, true);
		SummonTask->EventReceived.AddDynamic(this, &ThisClass::HandleSummonEvent);
		SummonTask->ReadyForActivation();
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SummonMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_SummonMinions::HandleSummonEvent(FGameplayEventData Payload)
{
	SummonMinions();
}

void UAIGameplayAbility_SummonMinions::HandleMontageEnded()
{
	// 노티파이가 없었으면 끝날 때 소환
	SummonMinions();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_SummonMinions::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_SummonMinions::SummonMinions()
{
	if (bSummonedThisActivation)
	{
		return;
	}

	bSummonedThisActivation = true;

	AWarriorAICharacter* Summoner = GetAICharacterFromActorInfo();

	if (!Summoner || !Summoner->HasAuthority())
	{
		return;
	}

	BindSummonerDeath();

	int32 CountToSpawn = FMath::Max(1, SummonCount);

	if (MaxAliveMinions > 0)
	{
		CountToSpawn = FMath::Min(CountToSpawn, MaxAliveMinions - GetAliveMinionCount());
	}

	AActor* SummonerTarget = FindAITargetActor(CurrentActorInfo);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Summoner;
	SpawnParams.Instigator = Summoner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < CountToSpawn; ++Index)
	{
		FVector SpawnLocation;

		if (!FindSummonLocation(Summoner->GetActorLocation(), SpawnLocation))
		{
			continue;
		}

		const FVector ToSpawn = (SpawnLocation - Summoner->GetActorLocation()).GetSafeNormal2D();
		const FRotator SpawnRotation = ToSpawn.IsNearlyZero() ? Summoner->GetActorRotation() : ToSpawn.Rotation();

		APawn* Minion = GetWorld()->SpawnActor<APawn>(MinionClass, SpawnLocation + FVector(0.f, 0.f, SpawnHeightOffset), SpawnRotation, SpawnParams);

		if (!Minion)
		{
			continue;
		}

		SummonedMinions.Add(Minion);

		//M9 난이도: 졸개는 웨이브 생성기를 거치지 않으므로 소환한 적(보스)의 배율을 그대로 받는다
		if (AWarriorAICharacter* MinionCharacter = Cast<AWarriorAICharacter>(Minion))
		{
			if (const AWarriorAICharacter* SummonerCharacter = Cast<AWarriorAICharacter>(Summoner))
			{
				MinionCharacter->SetDifficultyScale(SummonerCharacter->GetDifficultyHealthMultiplier(),
					SummonerCharacter->GetDifficultyDamageMultiplier());
			}
		}

		if (!Minion->GetController())
		{
			Minion->SpawnDefaultController();
		}

		// 감지 없이 소환자의 대상을 바로 노림
		if (AWarriorAIController* MinionController = Cast<AWarriorAIController>(Minion->GetController()))
		{
			if (SummonerTarget)
			{
				MinionController->SetInitialTarget(SummonerTarget);
			}
		}

		if (SummonEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SummonEffect, SpawnLocation, SpawnRotation);
		}

		PlayMinionEmerge(Minion);
	}
}

void UAIGameplayAbility_SummonMinions::PlayMinionEmerge(APawn* Minion) const
{
	ACharacter* MinionCharacter = Cast<ACharacter>(Minion);

	if (!MinionCharacter || !MinionEmergeMontage)
	{
		return;
	}

	const float MontageLength = MinionCharacter->PlayAnimMontage(MinionEmergeMontage);
	const float FreezeDuration = MinionEmergeDurationOverride > 0.f ? MinionEmergeDurationOverride : MontageLength;

	if (FreezeDuration <= 0.f)
	{
		return;
	}

	// 등장 중에는 제자리에서 나오도록 이동과 AI를 멈춤
	if (UCharacterMovementComponent* MovementComponent = MinionCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	AAIController* MinionController = Cast<AAIController>(MinionCharacter->GetController());

	if (MinionController && MinionController->GetBrainComponent())
	{
		MinionController->GetBrainComponent()->PauseLogic(TEXT("Emerge"));
	}

	// 소환자가 먼저 죽거나 어빌리티가 끝나도 하수인은 깨어나야 하므로 하수인 기준 타이머로 복구
	TWeakObjectPtr<ACharacter> WeakMinion = MinionCharacter;
	FTimerHandle TimerHandle;
	MinionCharacter->GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([WeakMinion]()
	{
		ACharacter* Character = WeakMinion.Get();

		// 등장 중에 죽었으면(소환자 사망 등) 이동·AI를 되살리지 않음
		if (!Character || Character->IsActorBeingDestroyed() || UWarriorFunctionLibrary::IsActorDead(Character))
		{
			return;
		}

		if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}

		if (AAIController* Controller = Cast<AAIController>(Character->GetController()))
		{
			if (UBrainComponent* BrainComponent = Controller->GetBrainComponent())
			{
				BrainComponent->ResumeLogic(TEXT("Emerge"));
			}
		}
	}), FreezeDuration, false);
}

void UAIGameplayAbility_SummonMinions::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (UAbilitySystemComponent* ASC = BoundSummonerASC.Get())
	{
		ASC->RegisterGameplayTagEvent(WarriorGameplayTags::Shared_Status_Death, EGameplayTagEventType::NewOrRemoved).Remove(SummonerDeathHandle);
	}

	SummonerDeathHandle.Reset();
	BoundSummonerASC.Reset();

	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UAIGameplayAbility_SummonMinions::BindSummonerDeath()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	if (!ASC || BoundSummonerASC.Get() == ASC)
	{
		return;
	}

	// 사망 태그(Normal/Knockback/Finisher)가 붙으면 부모 태그 Shared.Status.Death 카운트가 오름
	SummonerDeathHandle = ASC->RegisterGameplayTagEvent(WarriorGameplayTags::Shared_Status_Death, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleSummonerDeathTagChanged);
	BoundSummonerASC = ASC;
}

void UAIGameplayAbility_SummonMinions::HandleSummonerDeathTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		KillSummonedMinions();
	}
}

void UAIGameplayAbility_SummonMinions::KillSummonedMinions()
{
	// 사망 처리 중 다른 콜백이 목록을 건드릴 수 있으므로 복사해서 순회
	const TArray<TWeakObjectPtr<APawn>> MinionsToKill = SummonedMinions;
	SummonedMinions.Reset();

	for (const TWeakObjectPtr<APawn>& WeakMinion : MinionsToKill)
	{
		APawn* Minion = WeakMinion.Get();

		if (!Minion || Minion->IsActorBeingDestroyed() || UWarriorFunctionLibrary::IsActorDead(Minion))
		{
			continue;
		}

		UAbilitySystemComponent* MinionASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Minion);

		// 처형 연출 중인 하수인은 그 연출로 죽게 둠
		if (!MinionASC || MinionASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Finisher))
		{
			continue;
		}

		// 체력이 0이 될 때와 같은 경로: 사망 태그 → 사망 어빌리티 → OnCharacterDied
		UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(Minion, WarriorGameplayTags::Shared_Status_Death_Normal);
	}
}

int32 UAIGameplayAbility_SummonMinions::GetAliveMinionCount()
{
	SummonedMinions.RemoveAll([](const TWeakObjectPtr<APawn>& Minion)
	{
		return !Minion.IsValid() || Minion->IsActorBeingDestroyed();
	});

	return SummonedMinions.Num();
}

bool UAIGameplayAbility_SummonMinions::FindSummonLocation(const FVector& Origin, FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	// 최소 거리보다 가까운 점은 버리고 몇 번 다시 시도
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FVector Candidate;

		if (NavSystem)
		{
			FNavLocation NavLocation;

			if (!NavSystem->GetRandomReachablePointInRadius(Origin, SummonRadius, NavLocation))
			{
				continue;
			}

			Candidate = NavLocation.Location;
		}
		else
		{
			const FVector2D Offset = FMath::RandPointInCircle(SummonRadius);
			Candidate = Origin + FVector(Offset.X, Offset.Y, 0.f);
		}

		if (FVector::Dist2D(Candidate, Origin) >= MinSummonDistance)
		{
			OutLocation = Candidate;
			return true;
		}
	}

	return false;
}
