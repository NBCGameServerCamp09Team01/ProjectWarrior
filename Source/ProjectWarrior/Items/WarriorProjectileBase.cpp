// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorProjectileBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

AWarriorProjectileBase::AWarriorProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;

	InitialLifeSpan = 5.f;

	ProjectileCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision"));
	SetRootComponent(ProjectileCollision);
	ProjectileCollision->InitSphereRadius(5.f);
	ProjectileCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProjectileCollision->SetCollisionObjectType(ECC_WorldDynamic);
	ProjectileCollision->SetCollisionResponseToAllChannels(ECR_Block);
	ProjectileCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProjectileCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ProjectileCollision->SetGenerateOverlapEvents(true);
	ProjectileCollision->OnComponentHit.AddUniqueDynamic(this, &ThisClass::OnProjectileHit);
	ProjectileCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnProjectileBeginOverlap);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void AWarriorProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	// 쏜 캐릭터와 그 캐릭터에 붙은 무기에는 충돌하지 않음
	if (APawn* InstigatorPawn = GetInstigator())
	{
		ProjectileCollision->IgnoreActorWhenMoving(InstigatorPawn, true);

		TArray<AActor*> AttachedActors;
		InstigatorPawn->GetAttachedActors(AttachedActors);

		for (AActor* AttachedActor : AttachedActors)
		{
			ProjectileCollision->IgnoreActorWhenMoving(AttachedActor, true);
		}
	}
}

void AWarriorProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		HandleHitPawn(HitPawn, Hit.ImpactPoint);
		return;
	}

	BP_OnProjectileImpact(OtherActor, Hit.ImpactPoint, EWarriorHitResultType::Invalid);
	Destroy();
}

void AWarriorProjectileBase::OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		HandleHitPawn(HitPawn, bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation());
	}
}

void AWarriorProjectileBase::HandleHitPawn(APawn* HitPawn, const FVector& ImpactLocation)
{
	APawn* InstigatorPawn = GetInstigator();

	if (!InstigatorPawn || HitPawn == InstigatorPawn || ProcessedActors.Contains(HitPawn))
	{
		return;
	}

	ProcessedActors.AddUnique(HitPawn);

	// 아군은 통과
	if (!UWarriorFunctionLibrary::IsTargetPawnHostile(InstigatorPawn, HitPawn))
	{
		return;
	}

	if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitPawn))
	{
		BP_OnProjectileImpact(HitPawn, ImpactLocation, EWarriorHitResultType::Invalid);
		Destroy();
		return;
	}

	FGameplayEventData EventData;
	EventData.Instigator = InstigatorPawn;
	EventData.Target = HitPawn;

	const EWarriorHitResultType HitResult = UWarriorFunctionLibrary::EvaluateHitResult(InstigatorPawn, HitPawn, this);

	switch (HitResult)
	{
	case EWarriorHitResultType::Blocked:
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::Player_Event_Successful_Block, EventData);
		break;

	case EWarriorHitResultType::Dodged:
		// 회피 성공이면 화살은 계속 날아감
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::Player_Event_Successful_Dodge, EventData);
		return;

	case EWarriorHitResultType::Hit:
		ApplyDamageToTarget(HitPawn);

		if (bSendHitReactEvent)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::Shared_Event_HitReact, EventData);
		}
		break;

	default:
		return;
	}

	BP_OnProjectileImpact(HitPawn, ImpactLocation, HitResult);
	Destroy();
}

void AWarriorProjectileBase::ApplyDamageToTarget(AActor* TargetActor)
{
	checkf(ProjectileDamageEffectSpecHandle.IsValid(), TEXT("Forgot to assign a valid damage spec handle to the projectile: %s"), *GetName());

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*ProjectileDamageEffectSpecHandle.Data);
	}
}
