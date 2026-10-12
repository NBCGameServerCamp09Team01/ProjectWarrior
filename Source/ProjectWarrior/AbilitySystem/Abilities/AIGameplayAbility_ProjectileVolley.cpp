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

	if (bFaceTargetOnStart)
	{
		FaceTarget();
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
	const FVector SpawnLocation = GetSpawnLocation(SpawnBoneName);
	++FireCount;

	AActor* TargetActor = FindAITargetActor(CurrentActorInfo);

	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectClass
		? MakeAIDamageEffectSpecHandle(DamageEffectClass, DamageScalableFloat)
		: FGameplayEffectSpecHandle();

	const float ProjectileSpeed = ProjectileClass->GetDefaultObject<AWarriorProjectileBase>()->GetProjectileMovement()->InitialSpeed;
	const FVector CenterDirection = ComputeProjectileLaunchDirection(SpawnLocation, TargetActor, ProjectileSpeed, bPredictTargetMovement);

	const int32 Count = FMath::Max(1, ProjectilesPerFire);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// 가운데를 중심으로 좌우 대칭으로 펼침 (1발이면 0도)
		const float Alpha = Count > 1 ? static_cast<float>(Index) / static_cast<float>(Count - 1) : 0.5f;
		const float YawOffset = FMath::Lerp(-SpreadAngle * 0.5f, SpreadAngle * 0.5f, Alpha);
		const FVector LaunchDirection = CenterDirection.RotateAngleAxis(YawOffset, FVector::UpVector);

		if (AWarriorProjectileBase* Projectile = SpawnUnlaunchedProjectile(ProjectileClass, FTransform(LaunchDirection.Rotation(), SpawnLocation)))
		{
			Projectile->LaunchProjectile(LaunchDirection, DamageSpecHandle);
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
