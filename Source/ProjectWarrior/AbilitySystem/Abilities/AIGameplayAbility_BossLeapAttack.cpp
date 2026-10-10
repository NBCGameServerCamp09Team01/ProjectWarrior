// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_BossLeapAttack.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "DrawDebugHelpers.h"
#include "Character/ALSBaseCharacter.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "AbilitySystemComponent.h"

void UAIGameplayAbility_BossLeapAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 공중에서 취소(사망 등)되어도 이동 모드·충돌이 남지 않도록 복구
	EndLeapMovement();
	RemoveLeapWarpTarget();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAIGameplayAbility_BossLeapAttack::BeginLeap(AActor* TargetActor, float TelegraphDuration)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* ResolvedTarget = ResolveAreaTarget(TargetActor);

	if (!OwnerCharacter || !ResolvedTarget)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossLeapAttack] %s: leap target not found."), *GetName());
		return false;
	}

	const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
	const FVector TargetLocation = ResolvedTarget->GetActorLocation();

	FVector LeapDirection = (TargetLocation - OwnerLocation).GetSafeNormal2D();

	if (LeapDirection.IsNearlyZero())
	{
		LeapDirection = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	LeapRotation = LeapDirection.Rotation();
	bHasLeapRotation = true;

	const UCapsuleComponent* OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
	const ACharacter* TargetCharacter = Cast<ACharacter>(ResolvedTarget);
	const float TargetRadius = TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	const float TargetHalfHeight = TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;

	// 대상 앞에서 멈추도록 두 캡슐 반지름 + LandingGap만큼 덜 이동
	float LeapDistance = FVector::Dist2D(OwnerLocation, TargetLocation) - OwnerCapsule->GetScaledCapsuleRadius() - TargetRadius - LandingGap;
	LeapDistance = FMath::Max(LeapDistance, 0.f);

	if (MaxLeapDistance > 0.f)
	{
		LeapDistance = FMath::Min(LeapDistance, MaxLeapDistance);
	}

	// MotionWarping은 발 위치 기준(Warp To Feet Location). 착지 높이는 대상의 발 높이로 시작
	const FVector OwnerFeet = OwnerLocation - FVector(0.f, 0.f, OwnerCapsule->GetScaledCapsuleHalfHeight());

	LandingLocation = OwnerFeet + LeapDirection * LeapDistance;
	LandingLocation.Z = TargetLocation.Z - TargetHalfHeight;

	if (bProjectLandingToNavMesh)
	{
		LandingLocation = AdjustPointToNavigation(OwnerFeet, LandingLocation, NavProjectExtent);
	}

	if (UMotionWarpingComponent* MotionWarpingComponent = OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>())
	{
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, LandingLocation, LeapRotation);
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[BossLeapAttack] %s: MotionWarpingComponent not found on %s."), *GetName(), *OwnerCharacter->GetName());
	}

	// 판정 높이 기준은 캡슐 중심이므로 대상의 중심 높이 사용
	const FVector AreaOrigin = bCenterAreaOnLanding
		? FVector(LandingLocation.X, LandingLocation.Y, TargetLocation.Z)
		: TargetLocation;

	// Owner 앵커면 보스를 따라가 버리므로 고정 앵커로 강제
	FWarriorAttackAreaData LeapAreaData = DefaultAreaData;
	LeapAreaData.Anchor = EWarriorAttackAreaAnchor::TargetSnapshot;

	BeginAreaTelegraphAtAnchor(LeapAreaData, FTransform(LeapRotation, AreaOrigin), TelegraphDuration);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugArea)
	{
		DrawDebugSphere(GetWorld(), LandingLocation, 30.f, 12, FColor::Cyan, false, FMath::Max(TelegraphDuration, 0.1f), 0, 2.f);
		DrawDebugLine(GetWorld(), OwnerFeet, LandingLocation, FColor::Cyan, false, FMath::Max(TelegraphDuration, 0.1f), 0, 2.f);
	}
#endif

	return true;
}

void UAIGameplayAbility_BossLeapAttack::BeginLeapMovement()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (!OwnerCharacter)
	{
		return;
	}

	UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();

	if (bUseFlyingDuringLeap && !bAppliedFlying && MovementComponent)
	{
		SavedMovementMode = MovementComponent->MovementMode;
		SavedCustomMovementMode = MovementComponent->CustomMovementMode;

		MovementComponent->SetMovementMode(MOVE_Flying);
		bAppliedFlying = true;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();

	if (bIgnorePawnCollisionDuringLeap && !bAppliedCollisionIgnore && Capsule)
	{
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);

		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bAppliedCollisionIgnore = true;
	}

	// InAir 진입 시 ALS가 현재 방향을 공중 유지 방향으로 저장하므로 회전을 먼저 맞춤
	if (bFaceLeapDirectionOnTakeoff && bHasLeapRotation)
	{
		OwnerCharacter->SetActorRotation(LeapRotation);
	}

	if (bSetALSInAirDuringLeap && !bAppliedALSInAir)
	{
		if (AALSBaseCharacter* ALSCharacter = Cast<AALSBaseCharacter>(OwnerCharacter))
		{
			ALSCharacter->SetMovementState(EALSMovementState::InAir);
			bAppliedALSInAir = true;
		}
	}

	// 공중에서 페이즈 전환 연출로 끊기지 않도록 (AWarriorBossCharacter가 착지까지 미룸)
	if (!bAppliedAirborneTag)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(WarriorGameplayTags::AI_Status_Boss_Airborne);
			bAppliedAirborneTag = true;
		}
	}
}

void UAIGameplayAbility_BossLeapAttack::EndLeapMovement()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

	if (bAppliedAirborneTag)
	{
		bAppliedAirborneTag = false;

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(WarriorGameplayTags::AI_Status_Boss_Airborne);
		}
	}

	if (bAppliedFlying)
	{
		bAppliedFlying = false;

		if (UCharacterMovementComponent* MovementComponent = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr)
		{
			// 이전 모드가 없거나 비행이었으면 걷기로. 발밑에 바닥이 없으면 CharacterMovement가 낙하로 전환함
			const EMovementMode RestoreMode = (SavedMovementMode == MOVE_None || SavedMovementMode == MOVE_Flying) ? MOVE_Walking : SavedMovementMode.GetValue();

			MovementComponent->SetMovementMode(RestoreMode, SavedCustomMovementMode);
		}
	}

	if (bAppliedCollisionIgnore)
	{
		bAppliedCollisionIgnore = false;

		if (UCapsuleComponent* Capsule = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr)
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		}
	}

	if (bAppliedALSInAir)
	{
		bAppliedALSInAir = false;

		// Flying -> Walking 복구 때는 ALS가 OnMovementModeChanged로 Grounded를 설정하지만,
		// Flying을 쓰지 않았으면 모드 변경이 없으므로 직접 되돌림. 낙하 중이면 InAir 유지
		if (AALSBaseCharacter* ALSCharacter = Cast<AALSBaseCharacter>(OwnerCharacter))
		{
			const UCharacterMovementComponent* MovementComponent = ALSCharacter->GetCharacterMovement();

			if (MovementComponent && MovementComponent->IsMovingOnGround() && ALSCharacter->GetMovementState() == EALSMovementState::InAir)
			{
				ALSCharacter->SetMovementState(EALSMovementState::Grounded);
			}
		}
	}
}

void UAIGameplayAbility_BossLeapAttack::RemoveLeapWarpTarget()
{
	if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		if (UMotionWarpingComponent* MotionWarpingComponent = AvatarActor->FindComponentByClass<UMotionWarpingComponent>())
		{
			MotionWarpingComponent->RemoveWarpTarget(WarpTargetName);
		}
	}
}
