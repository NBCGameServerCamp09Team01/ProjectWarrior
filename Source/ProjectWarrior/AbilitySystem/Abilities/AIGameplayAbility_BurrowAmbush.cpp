// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BurrowAmbush.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"

namespace
{
	// 등장 위치를 예약한 어빌리티 인스턴스 (같은 자리에 겹쳐 나오지 않도록 서로 확인)
	TArray<TWeakObjectPtr<UAIGameplayAbility_BurrowAmbush>> GReservedAmbushes;
}

UAIGameplayAbility_BurrowAmbush::UAIGameplayAbility_BurrowAmbush()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UAIGameplayAbility_BurrowAmbush::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 이미 다른 개체들이 최대 수만큼 땅을 파고 있으면 발동하지 않음 (BT가 다른 패턴을 고르도록)
	if (MaxConcurrentBurrowers > 0 && BurrowingStatusTag.IsValid())
	{
		const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

		if (CountOtherBurrowers(AvatarActor) >= MaxConcurrentBurrowers)
		{
			return false;
		}
	}

	return true;
}

void UAIGameplayAbility_BurrowAmbush::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 부모가 토큰 부족 등으로 이미 끝냈으면 진행하지 않음
	if (!IsActive())
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter || !EmergeMontage)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BurrowAmbush] %s needs a character avatar and EmergeMontage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// EQS가 없거나 실패할 때 쓸 위치를 먼저 계산 (대상이 없으면 발동 실패)
	if (!ComputeEmergeLocation(EmergeFeetLocation, EmergeFacing))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bEmergeLocationLocked = false;
	SetBurrowingTag(true);

	// 파고드는 동안 EQS로 튀어나올 위치를 찾음. 땅속으로 들어가는 시점의 결과로 확정
	if (AmbushQuery)
	{
		FEnvQueryRequest QueryRequest(AmbushQuery, OwnerCharacter);
		// 전체 결과(점수순)를 받아 HandleAmbushQueryFinished에서 상위 후보 중 예약되지 않은 위치를 고름
		AmbushQueryRequestId = QueryRequest.Execute(EEnvQueryRunMode::AllMatching, FQueryFinishedSignature::CreateUObject(this, &ThisClass::HandleAmbushQueryFinished));
	}

	if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	float BurrowDuration = 0.f;

	if (BurrowMontage)
	{
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr)
		{
			const float PlayRate = FMath::Max(BurrowPlayRate, 0.1f);
			const float MontageLength = BurrowMontage->GetPlayLength();

			// 역재생은 끝에서 시작해야 함
			AnimInstance->Montage_Play(BurrowMontage, bPlayBurrowReversed ? -PlayRate : PlayRate, EMontagePlayReturnType::MontageLength,
				bPlayBurrowReversed ? MontageLength : 0.f);

			// 블렌드아웃이 시작되면 원래 자세로 돌아가므로 그 전에 숨김
			BurrowDuration = FMath::Max(MontageLength - BurrowMontage->GetDefaultBlendOutTime(), 0.f) / PlayRate;
		}
	}

	if (BurrowEffect)
	{
		const FVector OwnerFeet = OwnerCharacter->GetActorLocation() - FVector(0.f, 0.f, OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BurrowEffect, OwnerFeet, OwnerCharacter->GetActorRotation());
	}

	UAbilityTask_WaitDelay* BurrowTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(BurrowDuration, 0.01f));
	BurrowTask->OnFinish.AddDynamic(this, &ThisClass::HandleBurrowed);
	BurrowTask->ReadyForActivation();
}

void UAIGameplayAbility_BurrowAmbush::HandleBurrowed()
{
	// 이 시점의 위치로 확정 (아직 끝나지 않은 EQS는 버림). EQS 없이 확정된 위치도 예약
	bEmergeLocationLocked = true;
	AbortAmbushQuery();
	ReserveEmergeLocation();

	SetUnderground(true);

	// 판정 높이 기준은 캡슐 중심
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const float HalfHeight = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;

	FWarriorAttackAreaData AmbushAreaData = DefaultAreaData;
	AmbushAreaData.Anchor = EWarriorAttackAreaAnchor::TargetSnapshot;

	BeginAreaTelegraphAtAnchor(AmbushAreaData, FTransform(EmergeFacing, EmergeFeetLocation + FVector(0.f, 0.f, HalfHeight)), UndergroundDuration + EmergeImpactDelay);

	if (EmergeWarningEffect)
	{
		StopEmergeWarningEffect();
		EmergeWarningComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EmergeWarningEffect, EmergeFeetLocation, EmergeFacing);
	}

	UAbilityTask_WaitDelay* UndergroundTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(UndergroundDuration, 0.01f));
	UndergroundTask->OnFinish.AddDynamic(this, &ThisClass::HandleEmerge);
	UndergroundTask->ReadyForActivation();
}

void UAIGameplayAbility_BurrowAmbush::HandleEmerge()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float HalfHeight = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	OwnerCharacter->TeleportTo(EmergeFeetLocation + FVector(0.f, 0.f, HalfHeight), EmergeFacing, false, true);
	SetOwnerFacingRotation(EmergeFacing);

	SetUnderground(false);
	StopEmergeWarningEffect();

	if (EmergeEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EmergeEffect, EmergeFeetLocation, EmergeFacing);
	}

	UAbilityTask_WaitDelay* ImpactTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(EmergeImpactDelay, 0.01f));
	ImpactTask->OnFinish.AddDynamic(this, &ThisClass::HandleEmergeImpact);
	ImpactTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, EmergeMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleEmergeMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleEmergeMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleEmergeMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleEmergeMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_BurrowAmbush::HandleEmergeImpact()
{
	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	ApplyAreaDamage(DamageSpecHandle, true);
}

void UAIGameplayAbility_BurrowAmbush::HandleEmergeMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_BurrowAmbush::HandleEmergeMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_BurrowAmbush::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 땅속에서 취소(사망 등)되어도 숨김·충돌·이동·상태 태그가 남지 않도록 복구
	AbortAmbushQuery();
	SetUnderground(false);
	SetBurrowingTag(false);
	ReleaseEmergeLocation();
	StopEmergeWarningEffect();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_BurrowAmbush::HandleAmbushQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	AmbushQueryRequestId = INDEX_NONE;

	if (bEmergeLocationLocked || !IsActive())
	{
		return;
	}

	if (!Result.IsValid() || !Result->IsSuccessful() || Result->Items.IsEmpty())
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[BurrowAmbush] %s: ambush query found no location. Using fallback placement."), *GetName());
		return;
	}

	// 결과는 점수순. 상위 비율 안에서 무작위로, 다른 개체가 예약한 위치 근처는 건너뜀
	const int32 ItemCount = Result->Items.Num();
	const int32 TopCount = FMath::Clamp(FMath::CeilToInt(ItemCount * AmbushPickTopFraction), 1, ItemCount);

	TArray<int32> TopIndices;
	for (int32 Index = 0; Index < TopCount; ++Index)
	{
		TopIndices.Add(Index);
	}

	for (int32 Index = TopIndices.Num() - 1; Index > 0; --Index)
	{
		TopIndices.Swap(Index, FMath::RandRange(0, Index));
	}

	int32 PickedIndex = INDEX_NONE;

	for (const int32 Index : TopIndices)
	{
		if (!IsNearReservedLocation(Result->GetItemAsLocation(Index)))
		{
			PickedIndex = Index;
			break;
		}
	}

	// 상위가 모두 막혔으면 나머지에서 점수순으로
	for (int32 Index = TopCount; PickedIndex == INDEX_NONE && Index < ItemCount; ++Index)
	{
		if (!IsNearReservedLocation(Result->GetItemAsLocation(Index)))
		{
			PickedIndex = Index;
		}
	}

	if (PickedIndex == INDEX_NONE)
	{
		PickedIndex = TopIndices[0];
	}

	// 아이템은 내비메시에 투영된 지면 위치
	EmergeFeetLocation = Result->GetItemAsLocation(PickedIndex);
	EmergeFacing = ComputeFacingToTarget(EmergeFeetLocation);

	ReserveEmergeLocation();
}

bool UAIGameplayAbility_BurrowAmbush::IsNearReservedLocation(const FVector& Location) const
{
	for (const TWeakObjectPtr<UAIGameplayAbility_BurrowAmbush>& Reserved : GReservedAmbushes)
	{
		const UAIGameplayAbility_BurrowAmbush* Other = Reserved.Get();

		if (Other && Other != this && Other->bHasReservedLocation && Other->GetWorld() == GetWorld()
			&& FVector::Dist2D(Other->EmergeFeetLocation, Location) < MinEmergeSeparation)
		{
			return true;
		}
	}

	return false;
}

void UAIGameplayAbility_BurrowAmbush::ReserveEmergeLocation()
{
	GReservedAmbushes.RemoveAll([](const TWeakObjectPtr<UAIGameplayAbility_BurrowAmbush>& Reserved) { return !Reserved.IsValid(); });
	GReservedAmbushes.AddUnique(this);
	bHasReservedLocation = true;
}

void UAIGameplayAbility_BurrowAmbush::ReleaseEmergeLocation()
{
	bHasReservedLocation = false;
	GReservedAmbushes.Remove(this);
}

void UAIGameplayAbility_BurrowAmbush::StopEmergeWarningEffect()
{
	// 새 입자만 멈추고, 이미 튄 입자는 자연스럽게 떨어지도록 Deactivate (자동 파괴)
	if (EmergeWarningComponent)
	{
		EmergeWarningComponent->Deactivate();
		EmergeWarningComponent = nullptr;
	}
}

void UAIGameplayAbility_BurrowAmbush::AbortAmbushQuery()
{
	if (AmbushQueryRequestId == INDEX_NONE)
	{
		return;
	}

	if (UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(GetWorld()))
	{
		QueryManager->AbortQuery(AmbushQueryRequestId);
	}

	AmbushQueryRequestId = INDEX_NONE;
}

FRotator UAIGameplayAbility_BurrowAmbush::ComputeFacingToTarget(const FVector& FromLocation) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const AActor* TargetActor = FindAITargetActor(CurrentActorInfo);

	if (TargetActor)
	{
		const FVector ToTarget = (TargetActor->GetActorLocation() - FromLocation).GetSafeNormal2D();

		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget.Rotation();
		}
	}

	return AvatarActor ? AvatarActor->GetActorRotation() : FRotator::ZeroRotator;
}

int32 UAIGameplayAbility_BurrowAmbush::CountOtherBurrowers(const AActor* Self) const
{
	const UWorld* World = Self ? Self->GetWorld() : nullptr;

	if (!World || !BurrowingStatusTag.IsValid())
	{
		return 0;
	}

	int32 Count = 0;

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;

		if (Pawn == Self)
		{
			continue;
		}

		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
		{
			if (ASC->HasMatchingGameplayTag(BurrowingStatusTag))
			{
				++Count;
			}
		}
	}

	return Count;
}

void UAIGameplayAbility_BurrowAmbush::SetBurrowingTag(bool bEnable)
{
	if (bHasBurrowingTag == bEnable || !BurrowingStatusTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (bEnable)
		{
			ASC->AddLooseGameplayTag(BurrowingStatusTag);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(BurrowingStatusTag);
		}

		bHasBurrowingTag = bEnable;
	}
}

bool UAIGameplayAbility_BurrowAmbush::ComputeEmergeLocation(FVector& OutFeetLocation, FRotator& OutFacing)
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* TargetActor = ResolveAreaTarget(nullptr);

	if (!OwnerCharacter || !TargetActor)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BurrowAmbush] %s: ambush target not found."), *GetName());
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector TargetForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetRight = TargetActor->GetActorRightVector().GetSafeNormal2D();

	// 선호 방향을 먼저, 막히면 좌우 -> 대각선 뒤 -> 정면 순으로 시도
	const float SideSign = FMath::RandBool() ? 1.f : -1.f;
	TArray<FVector> Directions;

	switch (Placement)
	{
	case EWarriorAmbushPlacement::BesideTarget:
		Directions.Add(TargetRight * SideSign);
		break;

	case EWarriorAmbushPlacement::AroundTarget:
	{
		const FVector2D RandomDirection = FMath::RandPointInCircle(1.f).GetSafeNormal();
		Directions.Add(FVector(RandomDirection.X, RandomDirection.Y, 0.f));
		break;
	}

	case EWarriorAmbushPlacement::BehindTarget:
	default:
		Directions.Add(-TargetForward);
		break;
	}

	Directions.Add(TargetRight * SideSign);
	Directions.Add(-TargetRight * SideSign);
	Directions.Add((-TargetForward + TargetRight * SideSign).GetSafeNormal());
	Directions.Add((-TargetForward - TargetRight * SideSign).GetSafeNormal());
	Directions.Add(TargetForward);

	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	const float TargetHalfHeight = TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector TargetFeet = TargetLocation - FVector(0.f, 0.f, TargetHalfHeight);

	// 벽 너머 등 갈 수 없는 곳이면 대상 쪽으로 줄여서 내비메시 위에 둠. 너무 줄어들면(대상과 겹침) 다음 방향
	const float MinAcceptedDistance = AmbushDistance * 0.6f;
	float BestDistance = -1.f;

	for (const FVector& Direction : Directions)
	{
		const FVector Candidate = AdjustPointToNavigation(TargetFeet, TargetFeet + Direction * AmbushDistance, NavProjectExtent);
		const float CandidateDistance = FVector::Dist2D(Candidate, TargetFeet);

		if (CandidateDistance > BestDistance)
		{
			BestDistance = CandidateDistance;
			OutFeetLocation = Candidate;
		}

		if (CandidateDistance >= MinAcceptedDistance)
		{
			break;
		}
	}

	const FVector ToTarget = (TargetLocation - OutFeetLocation).GetSafeNormal2D();
	OutFacing = ToTarget.IsNearlyZero() ? OwnerCharacter->GetActorRotation() : ToTarget.Rotation();

	return true;
}

void UAIGameplayAbility_BurrowAmbush::SetUnderground(bool bUnderground)
{
	if (bIsUnderground == bUnderground)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter)
	{
		return;
	}

	bIsUnderground = bUnderground;

	// 땅속에서는 보이지 않고 맞지 않으며 움직이지 않음
	OwnerCharacter->SetActorHiddenInGame(bUnderground);
	OwnerCharacter->SetActorEnableCollision(!bUnderground);

	if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
	{
		if (bUnderground)
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->DisableMovement();
		}
		else
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}
}
