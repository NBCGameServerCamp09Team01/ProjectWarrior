// Fill out your copyright notice in the Description page of Project Settings.


#include "AIGameplayAbility_ProjectileVolley.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Items/WarriorProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

UAIGameplayAbility_ProjectileVolley::UAIGameplayAbility_ProjectileVolley()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	FireEventTag = WarriorGameplayTags::AI_Event_Projectile_Fire;
}

void UAIGameplayAbility_ProjectileVolley::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 부모가 토큰 부족 등으로 이미 끝냈으면 진행하지 않음
	if (!IsActive())
	{
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage || !ProjectileClass)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[ProjectileVolley] %s needs AttackMontage and ProjectileClass."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FireCount = 0;
	HeldProjectiles.Reset();

	if (bFaceTargetOnStart)
	{
		FaceTarget();
	}

	if (HoldEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* HoldTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HoldEventTag, nullptr, false, true);
		HoldTask->EventReceived.AddDynamic(this, &ThisClass::HandleHoldEvent);
		HoldTask->ReadyForActivation();
	}

	UAbilityTask_WaitGameplayEvent* FireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag, nullptr, false, true);
	FireTask->EventReceived.AddDynamic(this, &ThisClass::HandleFireEvent);
	FireTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void UAIGameplayAbility_ProjectileVolley::HandleFireEvent(FGameplayEventData Payload)
{
	AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo();

	if (!AICharacter)
	{
		return;
	}

	if (bFaceTargetOnFire)
	{
		FaceTarget();
	}

	FName SpawnBoneName = NAME_None;
	FVector SpawnLocation = GetSpawnLocation(SpawnBoneName);
	++FireCount;

	AActor* TargetActor = FindAITargetActor(CurrentActorInfo);

	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	// 들고 있는 투사체가 있으면 그것을 지금 위치에서 발사
	AWarriorProjectileBase* HeldProjectile = nullptr;

	while (!HeldProjectiles.IsEmpty() && !HeldProjectile)
	{
		HeldProjectile = HeldProjectiles[0].Get();
		HeldProjectiles.RemoveAt(0);
	}

	if (HeldProjectile)
	{
		SpawnLocation = HeldProjectile->GetActorLocation();

		FVector HeldDirection;
		float HeldSpeed = 0.f;
		ComputeLaunch(SpawnLocation, TargetActor, HeldDirection, HeldSpeed);
		HeldProjectile->LaunchProjectile(HeldDirection, DamageSpecHandle, HeldSpeed);

		UWarriorFunctionLibrary::SpawnFXAtLocation(this, ThrowEffect, SpawnLocation, HeldProjectile->GetActorRotation(), HeldProjectile->GetActorScale3D());
	}

	FVector CenterDirection;
	float LaunchSpeed = 0.f;
	ComputeLaunch(SpawnLocation, TargetActor, CenterDirection, LaunchSpeed);

	const int32 Count = HeldProjectile ? 0 : FMath::Max(1, ProjectilesPerFire);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// 가운데를 중심으로 좌우 대칭으로 펼침 (1발이면 0도)
		const float Alpha = Count > 1 ? static_cast<float>(Index) / static_cast<float>(Count - 1) : 0.5f;
		const float YawOffset = FMath::Lerp(-SpreadAngle * 0.5f, SpreadAngle * 0.5f, Alpha);
		const FVector LaunchDirection = CenterDirection.RotateAngleAxis(YawOffset, FVector::UpVector);

		if (AWarriorProjectileBase* Projectile = SpawnUnlaunchedProjectile(ProjectileClass, FTransform(LaunchDirection.Rotation(), SpawnLocation)))
		{
			Projectile->LaunchProjectile(LaunchDirection, DamageSpecHandle, LaunchSpeed);
		}
	}

	if (MuzzleEffect)
	{
		USkeletalMeshComponent* Mesh = AICharacter->GetMesh();

		if (Mesh && SpawnBoneName != NAME_None)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleEffect, Mesh, SpawnBoneName, FVector::ZeroVector, FRotator::ZeroRotator,
				MuzzleEffectScale, EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleEffect, SpawnLocation, CenterDirection.Rotation(), MuzzleEffectScale);
		}
	}
}

void UAIGameplayAbility_ProjectileVolley::ComputeLaunch(const FVector& LaunchLocation, AActor* TargetActor, FVector& OutDirection, float& OutSpeed)
{
	const UProjectileMovementComponent* MovementCDO = ProjectileClass->GetDefaultObject<AWarriorProjectileBase>()->GetProjectileMovement();
	const float ProjectileSpeed = MovementCDO->InitialSpeed;

	OutSpeed = ProjectileSpeed;
	OutDirection = ComputeProjectileLaunchDirection(LaunchLocation, TargetActor, ProjectileSpeed, bPredictTargetMovement);

	const UWorld* World = GetWorld();
	const float GravityZ = World ? World->GetGravityZ() * MovementCDO->ProjectileGravityScale : 0.f;

	if (!bUseArcTrajectory || !World || FMath::IsNearlyZero(GravityZ))
	{
		return;
	}

	AActor* ResolvedTarget = FindAITargetActor(CurrentActorInfo);

	if (TargetActor)
	{
		ResolvedTarget = TargetActor;
	}

	if (!ResolvedTarget)
	{
		return;
	}

	// 떨어질 위치 = 대상 위치 (+ 날아가는 동안의 이동 예측, 수평 거리 / 속도로 근사)
	FVector AimLocation = ResolvedTarget->GetActorLocation();

	if (bPredictTargetMovement && ProjectileSpeed > 0.f)
	{
		const float TravelTime = FVector::Dist2D(LaunchLocation, AimLocation) / ProjectileSpeed;
		AimLocation += ResolvedTarget->GetVelocity() * TravelTime;
	}

	FVector LaunchVelocity;

	const bool bSolved = UGameplayStatics::SuggestProjectileVelocity(this, LaunchVelocity, LaunchLocation, AimLocation, ProjectileSpeed,
		bFavorHighArc, 0.f, GravityZ, ESuggestProjVelocityTraceOption::DoNotTrace);

	// 너무 멀어 이 속도로 닿지 않으면 정해진 높이의 포물선으로 속도를 맞춤
	if (!bSolved && !UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, LaunchVelocity, LaunchLocation, AimLocation, GravityZ, ArcFallbackCurve))
	{
		return;
	}

	OutSpeed = LaunchVelocity.Size();
	OutDirection = LaunchVelocity.GetSafeNormal();
}

void UAIGameplayAbility_ProjectileVolley::HandleHoldEvent(FGameplayEventData Payload)
{
	AWarriorAICharacter* AICharacter = GetAICharacterFromActorInfo();

	if (!AICharacter || !IsActive())
	{
		return;
	}

	// 다음 발사에 쓸 뼈/소켓에 붙임 (발사할 때 FireCount가 같은 뼈를 가리킴)
	FName HoldBoneName = NAME_None;
	const FVector HoldLocation = GetSpawnLocation(HoldBoneName);

	AWarriorProjectileBase* Projectile = SpawnUnlaunchedProjectile(ProjectileClass, FTransform(AICharacter->GetActorRotation(), HoldLocation));

	if (!Projectile)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = AICharacter->GetMesh();

	if (Mesh && HoldBoneName != NAME_None)
	{
		Projectile->AttachToComponent(Mesh, bHoldInheritsOwnerScale ? FAttachmentTransformRules::SnapToTargetIncludingScale : FAttachmentTransformRules::SnapToTargetNotIncludingScale, HoldBoneName);
		Projectile->SetActorRelativeLocation(HoldOffset);
	}
	else
	{
		Projectile->AttachToActor(AICharacter, FAttachmentTransformRules::KeepWorldTransform);
	}

	HeldProjectiles.Add(Projectile);

	if (HoldGroundEffect)
	{
		// 소켓 위치의 발밑 높이에서 재생
		FVector GroundLocation = (Mesh && HoldGroundEffectSocket != NAME_None && Mesh->DoesSocketExist(HoldGroundEffectSocket))
			? Mesh->GetSocketLocation(HoldGroundEffectSocket)
			: HoldLocation;
		GroundLocation.Z = AICharacter->GetActorLocation().Z - AICharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		UWarriorFunctionLibrary::SpawnFXAtLocation(this, HoldGroundEffect, GroundLocation, AICharacter->GetActorRotation(),
			Mesh ? Mesh->GetComponentScale() : FVector::OneVector);
	}
}

void UAIGameplayAbility_ProjectileVolley::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 던지기 전에 끊기면 들고 있던 투사체 제거
	for (const TWeakObjectPtr<AWarriorProjectileBase>& HeldProjectile : HeldProjectiles)
	{
		if (AWarriorProjectileBase* Projectile = HeldProjectile.Get())
		{
			Projectile->Destroy();
		}
	}

	HeldProjectiles.Reset();

	// 목록에서 빠진 경우까지 대비해 몸에 붙어 있는 미발사 투사체도 정리
	if (HoldEventTag.IsValid())
	{
		if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
		{
			TArray<AActor*> AttachedActors;
			AvatarActor->GetAttachedActors(AttachedActors, true, true);

			for (AActor* AttachedActor : AttachedActors)
			{
				AWarriorProjectileBase* Projectile = Cast<AWarriorProjectileBase>(AttachedActor);

				if (Projectile && !Projectile->IsLaunched() && ProjectileClass && Projectile->IsA(ProjectileClass))
				{
					Projectile->Destroy();
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAIGameplayAbility_ProjectileVolley::HandleMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAIGameplayAbility_ProjectileVolley::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAIGameplayAbility_ProjectileVolley::FaceTarget()
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	AActor* TargetActor = FindAITargetActor(CurrentActorInfo);

	if (!AvatarPawn || !TargetActor)
	{
		return;
	}

	const FVector ToTarget = (TargetActor->GetActorLocation() - AvatarPawn->GetActorLocation()).GetSafeNormal2D();

	if (!ToTarget.IsNearlyZero())
	{
		UWarriorFunctionLibrary::SetPawnFacingRotation(AvatarPawn, ToTarget.Rotation());
	}
}

FVector UAIGameplayAbility_ProjectileVolley::GetSpawnLocation(FName& OutBoneName) const
{
	OutBoneName = NAME_None;

	const AWarriorAICharacter* AICharacter = Cast<AWarriorAICharacter>(GetAvatarActorFromActorInfo());

	if (!AICharacter)
	{
		return FVector::ZeroVector;
	}

	const USkeletalMeshComponent* Mesh = AICharacter->GetMesh();

	if (Mesh && !SpawnBoneNames.IsEmpty())
	{
		const FName BoneName = SpawnBoneNames[FireCount % SpawnBoneNames.Num()];

		if (Mesh->DoesSocketExist(BoneName))
		{
			OutBoneName = BoneName;
			return Mesh->GetSocketLocation(BoneName);
		}

		UE_LOG(LogProjectWarrior, Warning, TEXT("[ProjectileVolley] %s: bone/socket %s not found."), *GetName(), *BoneName.ToString());
	}

	return AICharacter->GetActorLocation() + AICharacter->GetActorForwardVector() * 100.f;
}
