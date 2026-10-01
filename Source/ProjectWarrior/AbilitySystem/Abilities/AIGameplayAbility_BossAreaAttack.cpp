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

TArray<AActor*> UAIGameplayAbility_BossAreaAttack::ApplyAreaDamage(const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bClearTelegraph)
{
	TArray<AActor*> HitActors;

	APawn* AttackerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	if (!bHasActiveArea || !AttackerPawn || !AttackerPawn->HasAuthority())
	{
		return HitActors;
	}

	const FTransform AreaTransform = GetCurrentAreaTransform();

	if (bDrawDebugArea)
	{
		CurrentAreaData.DrawDebug(GetWorld(), AreaTransform, FColor::Red, 1.f);
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossAreaAttack), false, AttackerPawn);

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		AreaTransform.GetLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(CurrentAreaData.GetBoundingRadius()),
		QueryParams
	);

	TSet<APawn*> ProcessedPawns;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		APawn* HitPawn = Cast<APawn>(OverlapResult.GetActor());

		if (!HitPawn || ProcessedPawns.Contains(HitPawn))
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

		if (!CurrentAreaData.IsInside(AreaTransform, HitPawn->GetActorLocation(), CapsuleRadius, CapsuleHalfHeight))
		{
			continue;
		}

		FGameplayEventData EventData;
		EventData.Instigator = AttackerPawn;
		EventData.Target = HitPawn;

		switch (UWarriorFunctionLibrary::EvaluateHitResult(AttackerPawn, HitPawn, nullptr, CurrentAreaData.bUnblockable))
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
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::Shared_Event_HitReact, EventData);
			}

			HitActors.Add(HitPawn);
			break;

		default:
			break;
		}
	}

	if (bClearTelegraph)
	{
		ClearAreaTelegraph();
	}

	return HitActors;
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
