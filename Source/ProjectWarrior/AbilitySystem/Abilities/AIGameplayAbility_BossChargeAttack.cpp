// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossChargeAttack.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "ProjectWarrior/ProjectWarrior.h"

void UAIGameplayAbility_BossChargeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 차징·돌진 중 취소(사망 등)되어도 충돌·틱이 남지 않도록 정리
	FinishDash(false);
	StopTick();

	ChargePhase = EBossChargePhase::None;
	ChargeTarget.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAIGameplayAbility_BossChargeAttack::BeginCharge(AActor* TargetActor, float ChargeDuration)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter || ChargePhase == EBossChargePhase::Dashing)
	{
		return false;
	}

	AActor* ResolvedTarget = ResolveAreaTarget(TargetActor);
	ChargeTarget = ResolvedTarget;

	if (!ResolvedTarget)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossChargeAttack] %s: charge target not found. Charging forward."), *GetName());
	}

	// Focus가 남아 있으면 컨트롤러 회전이 직접 지정한 회전과 다투므로 해제
	if (AAIController* AIController = Cast<AAIController>(OwnerCharacter->GetController()))
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	// Owner 앵커: 표시가 보스에 붙어 보스가 회전하면 함께 회전
	FWarriorAttackAreaData ChargeAreaData = MakeDashAreaData(DashDistance + OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius());
	ChargeAreaData.Anchor = EWarriorAttackAreaAnchor::Owner;

	const FTransform OwnerTransform(FRotator(0.f, OwnerCharacter->GetActorRotation().Yaw, 0.f), OwnerCharacter->GetActorLocation());
	BeginAreaTelegraphAtAnchor(ChargeAreaData, OwnerTransform, ChargeDuration);

	ChargeElapsedTime = 0.f;
	ChargeLockTime = FMath::Max(ChargeDuration - LockBeforeDashTime, 0.f);
	ChargePhase = ResolvedTarget ? EBossChargePhase::Tracking : EBossChargePhase::Locked;

	ScheduleNextTick();

	return ResolvedTarget != nullptr;
}

bool UAIGameplayAbility_BossChargeAttack::StartDash(const FGameplayEffectSpecHandle& InDamageSpecHandle, float DashDuration)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter || ChargePhase == EBossChargePhase::Dashing)
	{
		return false;
	}

	const UCapsuleComponent* OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
	const float OwnerRadius = OwnerCapsule->GetScaledCapsuleRadius();
	const float OwnerHalfHeight = OwnerCapsule->GetScaledCapsuleHalfHeight();

	DashRotation = FRotator(0.f, OwnerCharacter->GetActorRotation().Yaw, 0.f);
	const FVector DashDirection = DashRotation.Vector();

	const FVector StartLocation = OwnerCharacter->GetActorLocation();
	const FVector StartFeet = StartLocation - FVector(0.f, 0.f, OwnerHalfHeight);

	FVector EndFeet = StartFeet + DashDirection * DashDistance;

	if (bProjectDashToNavMesh)
	{
		EndFeet = AdjustPointToNavigation(StartFeet, EndFeet, NavProjectExtent);
	}

	const FVector EndLocation = EndFeet + FVector(0.f, 0.f, OwnerHalfHeight);
	const float DashLength = FVector::Dist2D(StartLocation, EndLocation);

	// 실제 경로에 표시 고정 (가득 찬 상태로)
	if (bKeepTelegraphDuringDash)
	{
		FWarriorAttackAreaData DashAreaData = MakeDashAreaData(DashLength + OwnerRadius);
		DashAreaData.Anchor = EWarriorAttackAreaAnchor::OwnerSnapshot;

		BeginAreaTelegraphAtAnchor(DashAreaData, FTransform(DashRotation, StartLocation), 0.f);
	}
	else
	{
		ClearAreaTelegraph();
	}

	if (bIgnorePawnCollisionDuringDash && !bAppliedCollisionIgnore)
	{
		UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();

		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bAppliedCollisionIgnore = true;
	}

	SetOwnerFacingRotation(DashRotation);

	DashDamageSpecHandle = InDamageSpecHandle;
	DashProcessedActors.Reset();
	PreviousDashLocation = StartLocation;
	ChargePhase = EBossChargePhase::Dashing;

	DashMoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this,
		NAME_None,
		EndLocation,
		FMath::Max(DashDuration, 0.05f),
		false,
		MOVE_Walking,
		true,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.f
	);

	if (DashMoveTask)
	{
		DashMoveTask->OnTimedOut.AddDynamic(this, &ThisClass::HandleDashMoveFinished);
		DashMoveTask->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::HandleDashMoveFinished);
		DashMoveTask->ReadyForActivation();
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugArea)
	{
		DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Cyan, false, FMath::Max(DashDuration, 0.1f) + 1.f, 0, 3.f);
	}
#endif

	ScheduleNextTick();

	return true;
}

void UAIGameplayAbility_BossChargeAttack::TickCharge()
{
	ChargeTickHandle.Invalidate();

	const UWorld* World = GetWorld();

	if (!World || !IsActive())
	{
		return;
	}

	const float DeltaTime = World->GetDeltaSeconds();

	switch (ChargePhase)
	{
	case EBossChargePhase::Tracking:
		ChargeElapsedTime += DeltaTime;
		RotateTowardTarget(DeltaTime);

		if (ChargeElapsedTime >= ChargeLockTime)
		{
			ChargePhase = EBossChargePhase::Locked;
		}
		break;

	case EBossChargePhase::Locked:
		// 방향 고정 상태로 StartDash 대기
		return;

	case EBossChargePhase::Dashing:
		ApplyDashSweepDamage();
		break;

	default:
		return;
	}

	ScheduleNextTick();
}

void UAIGameplayAbility_BossChargeAttack::ScheduleNextTick()
{
	UWorld* World = GetWorld();

	if (World && !ChargeTickHandle.IsValid())
	{
		ChargeTickHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::TickCharge));
	}
}

void UAIGameplayAbility_BossChargeAttack::StopTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTickHandle);
	}

	ChargeTickHandle.Invalidate();
}

void UAIGameplayAbility_BossChargeAttack::RotateTowardTarget(float DeltaTime)
{
	const AActor* OwnerActor = GetAvatarActorFromActorInfo();
	const AActor* TargetActor = ChargeTarget.Get();

	if (!OwnerActor || !TargetActor)
	{
		return;
	}

	const FVector ToTarget = (TargetActor->GetActorLocation() - OwnerActor->GetActorLocation()).GetSafeNormal2D();

	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentRotation(0.f, OwnerActor->GetActorRotation().Yaw, 0.f);
	const FRotator DesiredRotation = ToTarget.Rotation();

	const FRotator NewRotation = TrackingRotationSpeed > 0.f
		? FMath::RInterpConstantTo(CurrentRotation, DesiredRotation, DeltaTime, TrackingRotationSpeed)
		: DesiredRotation;

	SetOwnerFacingRotation(NewRotation);
}

void UAIGameplayAbility_BossChargeAttack::ApplyDashSweepDamage()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter)
	{
		return;
	}

	const float OwnerRadius = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector CurrentLocation = OwnerCharacter->GetActorLocation();
	const FVector DashDirection = DashRotation.Vector();

	// 직전 위치의 캡슐 뒤쪽 끝부터 현재 위치의 캡슐 앞 + DashHitForwardReach까지
	const float SweepLength = FVector::Dist2D(PreviousDashLocation, CurrentLocation) + OwnerRadius * 2.f + DashHitForwardReach;
	const FTransform SweepTransform(DashRotation, PreviousDashLocation - DashDirection * OwnerRadius);

	FWarriorAttackAreaData SweepAreaData = MakeDashAreaData(SweepLength);
	SweepAreaData.LocalOffset = FVector::ZeroVector;

	ApplyAreaDamageAt(SweepAreaData, SweepTransform, DashDamageSpecHandle, &DashProcessedActors);

	if (bDrawDebugArea)
	{
		SweepAreaData.DrawDebug(GetWorld(), SweepTransform, FColor::Red, 0.f);
	}

	PreviousDashLocation = CurrentLocation;
}

void UAIGameplayAbility_BossChargeAttack::HandleDashMoveFinished()
{
	FinishDash(true);
}

void UAIGameplayAbility_BossChargeAttack::FinishDash(bool bNotifyBlueprint)
{
	if (ChargePhase != EBossChargePhase::Dashing)
	{
		return;
	}

	// 정상 종료면 마지막 구간 판정 (취소 시에는 판정하지 않음)
	if (bNotifyBlueprint)
	{
		ApplyDashSweepDamage();
	}

	ChargePhase = EBossChargePhase::None;
	StopTick();

	if (DashMoveTask)
	{
		DashMoveTask->OnTimedOut.RemoveAll(this);
		DashMoveTask->OnTimedOutAndDestinationReached.RemoveAll(this);
		DashMoveTask->EndTask();
		DashMoveTask = nullptr;
	}

	RestorePawnCollision();
	ClearAreaTelegraph();

	DashDamageSpecHandle = FGameplayEffectSpecHandle();
	DashProcessedActors.Reset();

	if (bNotifyBlueprint)
	{
		OnChargeDashFinished();
	}
}

void UAIGameplayAbility_BossChargeAttack::RestorePawnCollision()
{
	if (!bAppliedCollisionIgnore)
	{
		return;
	}

	bAppliedCollisionIgnore = false;

	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		OwnerCharacter->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
	}
}

FWarriorAttackAreaData UAIGameplayAbility_BossChargeAttack::MakeDashAreaData(float InLength) const
{
	FWarriorAttackAreaData AreaData = DefaultAreaData;
	AreaData.Shape = EWarriorAttackAreaShape::Box;
	AreaData.Length = FMath::Max(InLength, 1.f);

	return AreaData;
}
