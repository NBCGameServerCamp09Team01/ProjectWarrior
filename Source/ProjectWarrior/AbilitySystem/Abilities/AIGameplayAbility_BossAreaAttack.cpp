// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossAreaAttack.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Items/WarriorAttackIndicator.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Character/ALSBaseCharacter.h"
#include "DrawDebugHelpers.h"
#include "ProjectWarrior/ProjectWarrior.h"

namespace
{
	// AI 컨트롤러 블랙보드의 TargetActor 키 (AWarriorAIController::OnPerceptionUpdated가 채움)
	AActor* GetBlackboardTargetActor(const AActor* AvatarActor)
	{
		const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
		const AAIController* AIController = AvatarPawn ? Cast<AAIController>(AvatarPawn->GetController()) : nullptr;
		const UBlackboardComponent* BlackboardComponent = AIController ? AIController->GetBlackboardComponent() : nullptr;

		return BlackboardComponent ? Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetActor"))) : nullptr;
	}
}

UAIGameplayAbility_BossAreaAttack::UAIGameplayAbility_BossAreaAttack()
{
	// 보스 범위 공격은 플레이어의 Light 쿨다운을 무시하고 항상 반응하도록 Heavy
	HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_Heavy;

	MontageImpactEventTag = WarriorGameplayTags::AI_Event_Boss_AreaImpact;
}

void UAIGameplayAbility_BossAreaAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 판정 전에 취소(그로기 등)되어도 표시가 남지 않도록 제거
	ClearAreaTelegraph();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_BossAreaAttack::BeginAreaTelegraph(const FWarriorAttackAreaData& InAreaData, AActor* TargetActor, float TelegraphDuration)
{
	if (!GetAvatarActorFromActorInfo())
	{
		return;
	}

	BeginAreaTelegraphAtAnchor(InAreaData, ComputeAnchorTransform(InAreaData.Anchor, TargetActor), TelegraphDuration);
}

void UAIGameplayAbility_BossAreaAttack::BeginAreaTelegraphAtAnchor(const FWarriorAttackAreaData& InAreaData, const FTransform& AnchorTransform, float TelegraphDuration)
{
	ClearAreaTelegraph();

	AActor* AvatarActor = GetAvatarActorFromActorInfo();

	if (!AvatarActor)
	{
		return;
	}

	CurrentAreaData = InAreaData;
	bHasActiveArea = true;

	SnapshotAreaTransform = FTransform(AnchorTransform.GetRotation(), AnchorTransform.TransformPositionNoScale(InAreaData.LocalOffset));

	if (IndicatorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = AvatarActor;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AWarriorAttackIndicator* Indicator = GetWorld()->SpawnActor<AWarriorAttackIndicator>(IndicatorClass, SnapshotAreaTransform, SpawnParams))
		{
			if (InAreaData.Anchor == EWarriorAttackAreaAnchor::Owner)
			{
				Indicator->AttachToActor(AvatarActor, FAttachmentTransformRules::KeepWorldTransform);
			}

			Indicator->InitializeIndicator(InAreaData, TelegraphDuration);
			ActiveIndicator = Indicator;
		}
	}

	if (bDrawDebugArea)
	{
		CurrentAreaData.DrawDebug(GetWorld(), GetCurrentAreaTransform(), FColor::Yellow, FMath::Max(TelegraphDuration, 0.1f));
	}
}

void UAIGameplayAbility_BossAreaAttack::BeginDefaultAreaTelegraph(AActor* TargetActor, float TelegraphDuration)
{
	BeginAreaTelegraph(DefaultAreaData, TargetActor, TelegraphDuration);
}

bool UAIGameplayAbility_BossAreaAttack::BeginMontageAreaTelegraph(UAnimMontage* Montage, AActor* TargetActor, bool bFaceTarget, float TelegraphDuration)
{
	if (!Montage)
	{
		return false;
	}

	float ImpactTime = 0.f;

	if (!FindMontageEventTime(Montage, MontageImpactEventTag, ImpactTime))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossAreaAttack] %s: notify with %s not found in %s."), *GetName(), *MontageImpactEventTag.ToString(), *Montage->GetName());
		return false;
	}

	return BeginMontageAreaTelegraphAtTime(Montage, ImpactTime, DefaultAreaData, TargetActor, bFaceTarget, TelegraphDuration);
}

bool UAIGameplayAbility_BossAreaAttack::BeginMontageAreaTelegraphAtTime(UAnimMontage* Montage, float ImpactTime, const FWarriorAttackAreaData& InAreaData, AActor* TargetActor, bool bFaceTarget, float TelegraphDuration)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter || !Montage)
	{
		return false;
	}

	if (bFaceTarget)
	{
		FaceAreaTarget(TargetActor);
	}

	// 이미 재생 중이면 현재 위치부터 (그 사이 이동한 만큼은 이미 액터 위치에 반영됨)
	float CurrentPosition = 0.f;
	float PlayRate = 1.f;

	if (const UAnimInstance* AnimInstance = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr)
	{
		if (AnimInstance->Montage_IsPlaying(Montage))
		{
			CurrentPosition = AnimInstance->Montage_GetPosition(Montage);
			PlayRate = FMath::Abs(AnimInstance->Montage_GetPlayRate(Montage));
		}
	}

	PlayRate *= FMath::Max(Montage->RateScale, KINDA_SMALL_NUMBER);

	const FRotator OwnerYawRotation(0.f, OwnerCharacter->GetActorRotation().Yaw, 0.f);
	FTransform ImpactAnchor(OwnerYawRotation, OwnerCharacter->GetActorLocation());

	if (ImpactTime > CurrentPosition && Montage->HasRootMotion())
	{
		// 메시 기준 루트 모션 -> 월드 이동량 (메시의 회전 오프셋 반영)
		const FTransform LocalRootMotion = Montage->ExtractRootMotionFromTrackRange(CurrentPosition, ImpactTime, FAnimExtractContext());
		const FTransform WorldDelta = OwnerCharacter->GetMesh()->ConvertLocalRootMotionToWorld(LocalRootMotion);

		const FVector DeltaTranslation(WorldDelta.GetTranslation().X, WorldDelta.GetTranslation().Y, 0.f);
		const float ImpactYaw = (WorldDelta.GetRotation() * OwnerYawRotation.Quaternion()).Rotator().Yaw;

		ImpactAnchor = FTransform(FRotator(0.f, ImpactYaw, 0.f), OwnerCharacter->GetActorLocation() + DeltaTranslation);
	}

	if (TelegraphDuration < 0.f)
	{
		TelegraphDuration = FMath::Max(ImpactTime - CurrentPosition, 0.f) / PlayRate;
	}

	// Owner 앵커면 보스를 따라가 버리므로 고정 앵커로 강제
	FWarriorAttackAreaData MontageAreaData = InAreaData;
	MontageAreaData.Anchor = EWarriorAttackAreaAnchor::OwnerSnapshot;

	BeginAreaTelegraphAtAnchor(MontageAreaData, ImpactAnchor, TelegraphDuration);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugArea)
	{
		DrawDebugLine(GetWorld(), OwnerCharacter->GetActorLocation(), ImpactAnchor.GetLocation(), FColor::Cyan, false, FMath::Max(TelegraphDuration, 0.1f), 0, 2.f);
		DrawDebugSphere(GetWorld(), ImpactAnchor.GetLocation(), 30.f, 12, FColor::Cyan, false, FMath::Max(TelegraphDuration, 0.1f), 0, 2.f);
	}
#endif

	return true;
}

bool UAIGameplayAbility_BossAreaAttack::FaceAreaTarget(AActor* TargetActor)
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const AActor* ResolvedTarget = ResolveAreaTarget(TargetActor);

	if (!AvatarActor || !ResolvedTarget)
	{
		return false;
	}

	const FVector ToTarget = (ResolvedTarget->GetActorLocation() - AvatarActor->GetActorLocation()).GetSafeNormal2D();

	if (!ToTarget.IsNearlyZero())
	{
		SetOwnerFacingRotation(ToTarget.Rotation());
	}

	return true;
}

void UAIGameplayAbility_BossAreaAttack::SetOwnerFacingRotation(const FRotator& NewRotation) const
{
	UWarriorFunctionLibrary::SetPawnFacingRotation(Cast<APawn>(GetAvatarActorFromActorInfo()), NewRotation);
}

bool UAIGameplayAbility_BossAreaAttack::FindMontageEventTime(const UAnimMontage* InMontage, const FGameplayTag& InEventTag, float& OutTime)
{
	TArray<float> EventTimes;
	FindMontageEventTimes(InMontage, InEventTag, EventTimes);

	if (EventTimes.IsEmpty())
	{
		return false;
	}

	OutTime = EventTimes[0];
	return true;
}

void UAIGameplayAbility_BossAreaAttack::FindMontageEventTimes(const UAnimMontage* InMontage, const FGameplayTag& InEventTag, TArray<float>& OutTimes)
{
	OutTimes.Reset();

	if (!InMontage || !InEventTag.IsValid())
	{
		return;
	}

	for (const FAnimNotifyEvent& NotifyEvent : InMontage->Notifies)
	{
		const UObject* NotifyObject = NotifyEvent.Notify ? static_cast<const UObject*>(NotifyEvent.Notify) : static_cast<const UObject*>(NotifyEvent.NotifyStateClass);

		if (!NotifyObject)
		{
			continue;
		}

		// BP 노티파이의 이벤트 태그 변수 이름은 제각각이므로 GameplayTag 변수 값을 모두 비교
		for (TFieldIterator<FStructProperty> It(NotifyObject->GetClass()); It; ++It)
		{
			if (It->Struct != FGameplayTag::StaticStruct())
			{
				continue;
			}

			if (*It->ContainerPtrToValuePtr<FGameplayTag>(NotifyObject) == InEventTag)
			{
				OutTimes.Add(NotifyEvent.GetTriggerTime());
				break;
			}
		}
	}

	// 노티파이 배열은 트랙 순서라 시간순이 아닐 수 있음
	OutTimes.Sort();
}

TArray<AActor*> UAIGameplayAbility_BossAreaAttack::ApplyAreaDamage(const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bClearTelegraph)
{
	TArray<AActor*> HitActors;

	if (bHasActiveArea)
	{
		const FTransform AreaTransform = GetCurrentAreaTransform();

		if (bDrawDebugArea)
		{
			CurrentAreaData.DrawDebug(GetWorld(), AreaTransform, FColor::Red, 1.f);
		}

		HitActors = ApplyAreaDamageAt(CurrentAreaData, AreaTransform, InDamageSpecHandle, nullptr);
	}

	if (bClearTelegraph)
	{
		ClearAreaTelegraph();
	}

	return HitActors;
}

TArray<AActor*> UAIGameplayAbility_BossAreaAttack::ApplyAreaDamageAt(const FWarriorAttackAreaData& InAreaData, const FTransform& InAreaTransform, const FGameplayEffectSpecHandle& InDamageSpecHandle, TSet<TWeakObjectPtr<AActor>>* InOutProcessedActors)
{
	TArray<AActor*> HitActors;

	APawn* AttackerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	if (!AttackerPawn || !AttackerPawn->HasAuthority())
	{
		return HitActors;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossAreaAttack), false, AttackerPawn);

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		InAreaTransform.GetLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(InAreaData.GetBoundingRadius()),
		QueryParams
	);

	TSet<APawn*> ProcessedPawns;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		APawn* HitPawn = Cast<APawn>(OverlapResult.GetActor());

		if (!HitPawn || ProcessedPawns.Contains(HitPawn) || (InOutProcessedActors && InOutProcessedActors->Contains(HitPawn)))
		{
			continue;
		}

		ProcessedPawns.Add(HitPawn);

		if (!UWarriorFunctionLibrary::IsTargetPawnHostile(AttackerPawn, HitPawn))
		{
			continue;
		}

		float CapsuleRadius = 0.f;
		float CapsuleHalfHeight = 0.f;

		if (ACharacter* HitCharacter = Cast<ACharacter>(HitPawn))
		{
			HitCharacter->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
		}

		if (!InAreaData.IsInside(InAreaTransform, HitPawn->GetActorLocation(), CapsuleRadius, CapsuleHalfHeight))
		{
			continue;
		}

		if (InOutProcessedActors)
		{
			InOutProcessedActors->Add(HitPawn);
		}

		FGameplayEventData EventData;
		EventData.Instigator = AttackerPawn;
		EventData.Target = HitPawn;

		switch (UWarriorFunctionLibrary::EvaluateHitResult(AttackerPawn, HitPawn, nullptr, InAreaData.bUnblockable ? EWarriorBlockRule::Unblockable : EWarriorBlockRule::Blockable))
		{
		case EWarriorHitResultType::Blocked:
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::Player_Event_Successful_Block, EventData);
			break;

		case EWarriorHitResultType::Dodged:
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::Player_Event_Successful_Dodge, EventData);
			break;

		case EWarriorHitResultType::Hit:
			if (InDamageSpecHandle.IsValid() && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitPawn))
			{
				NativeApplyEffectSpecHandleToTarget(HitPawn, InDamageSpecHandle);
			}

			if (bSendHitReactEvent)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, HitReactEventTag.IsValid() ? HitReactEventTag : WarriorGameplayTags::Shared_Event_HitReact_Heavy, EventData);
			}

			HitActors.Add(HitPawn);
			break;

		default:
			break;
		}
	}

	return HitActors;
}

FVector UAIGameplayAbility_BossAreaAttack::AdjustPointToNavigation(const FVector& InStartPoint, const FVector& InDesiredPoint, const FVector& InProjectExtent) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (!NavSystem)
	{
		return InDesiredPoint;
	}

	FNavLocation ProjectedStart;
	FNavLocation ProjectedDesired;

	if (!NavSystem->ProjectPointToNavigation(InDesiredPoint, ProjectedDesired, InProjectExtent))
	{
		return InDesiredPoint;
	}

	FVector AdjustedPoint = ProjectedDesired.Location;

	// 시작점 -> 목표 사이 내비메시가 끊겨 있으면(벽, 낭떠러지) 끊긴 지점까지로 줄임
	if (NavSystem->ProjectPointToNavigation(InStartPoint, ProjectedStart, InProjectExtent))
	{
		FVector HitLocation;

		if (UNavigationSystemV1::NavigationRaycast(GetWorld(), ProjectedStart.Location, AdjustedPoint, HitLocation))
		{
			AdjustedPoint = HitLocation;
		}
	}

	return AdjustedPoint;
}

void UAIGameplayAbility_BossAreaAttack::ClearAreaTelegraph()
{
	if (AWarriorAttackIndicator* Indicator = ActiveIndicator.Get())
	{
		Indicator->Destroy();
	}

	ActiveIndicator.Reset();
	bHasActiveArea = false;
}

FTransform UAIGameplayAbility_BossAreaAttack::GetCurrentAreaTransform() const
{
	if (bHasActiveArea && CurrentAreaData.Anchor == EWarriorAttackAreaAnchor::Owner)
	{
		if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
		{
			const FTransform OwnerTransform(FRotator(0.f, AvatarActor->GetActorRotation().Yaw, 0.f), AvatarActor->GetActorLocation());

			return FTransform(OwnerTransform.GetRotation(), OwnerTransform.TransformPositionNoScale(CurrentAreaData.LocalOffset));
		}
	}

	return SnapshotAreaTransform;
}

AActor* UAIGameplayAbility_BossAreaAttack::ResolveAreaTarget(AActor* TargetActor)
{
	if (AActor* ResolvedTarget = ResolveProjectileTarget(TargetActor))
	{
		return ResolvedTarget;
	}

	return GetBlackboardTargetActor(GetAvatarActorFromActorInfo());
}

FTransform UAIGameplayAbility_BossAreaAttack::ComputeAnchorTransform(EWarriorAttackAreaAnchor InAnchor, AActor* TargetActor)
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	check(AvatarActor);

	const FVector OwnerLocation = AvatarActor->GetActorLocation();
	const FRotator OwnerYawRotation(0.f, AvatarActor->GetActorRotation().Yaw, 0.f);

	if (InAnchor == EWarriorAttackAreaAnchor::TargetSnapshot)
	{
		AActor* ResolvedTarget = ResolveAreaTarget(TargetActor);

		if (!ResolvedTarget)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[BossAreaAttack] %s: TargetSnapshot target not found. Falling back to owner location."), *GetName());
		}

		if (ResolvedTarget)
		{
			const FVector TargetLocation = ResolvedTarget->GetActorLocation();
			const FVector ToTarget = (TargetLocation - OwnerLocation).GetSafeNormal2D();

			const FRotator AreaRotation = ToTarget.IsNearlyZero() ? OwnerYawRotation : ToTarget.Rotation();

			return FTransform(AreaRotation, TargetLocation);
		}
	}

	return FTransform(OwnerYawRotation, OwnerLocation);
}
