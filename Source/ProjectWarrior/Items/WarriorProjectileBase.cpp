// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"

AWarriorProjectileBase::AWarriorProjectileBase()
{
	// 발사 후에만 틱 (환경 충돌 트레이스)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ProjectileRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileRoot"));
	SetRootComponent(ProjectileRoot);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("ProjectileCollision"));
	ProjectileCollision->SetupAttachment(RootComponent);
	ProjectileCollision->SetBoxExtent(FVector(25.f, 3.f, 3.f));
	// 발사 전(대기 상태)에는 충돌하지 않음. LaunchProjectile에서 켬
	ProjectileCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileCollision->SetCollisionObjectType(ECC_WorldDynamic);
	ProjectileCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	ProjectileCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ProjectileCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	ProjectileCollision->SetGenerateOverlapEvents(true);
	ProjectileCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnProjectileBeginOverlap);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bAutoActivate = false;
}

void AWarriorProjectileBase::LaunchProjectile(const FVector& LaunchDirection, const FGameplayEffectSpecHandle& InDamageSpecHandle)
{
	if (bLaunched)
	{
		return;
	}

	bLaunched = true;

	if (InDamageSpecHandle.IsValid())
	{
		ProjectileDamageEffectSpecHandle = InDamageSpecHandle;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	const FVector Direction = LaunchDirection.IsNearlyZero() ? GetActorForwardVector() : LaunchDirection.GetSafeNormal();
	SetActorRotation(Direction.Rotation());

	LastTraceLocation = GetActorLocation();

	ProjectileCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	ProjectileMovement->SetUpdatedComponent(ProjectileRoot);
	ProjectileMovement->Velocity = Direction * ProjectileMovement->InitialSpeed;
	ProjectileMovement->Activate(true);

	SetActorTickEnabled(true);
	SetLifeSpan(LifeSpanAfterLaunch);
}

void AWarriorProjectileBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector CurrentLocation = GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WarriorProjectileTrace), false, this);

	if (APawn* InstigatorPawn = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);

		TArray<AActor*> AttachedActors;
		InstigatorPawn->GetAttachedActors(AttachedActors);
		QueryParams.AddIgnoredActors(AttachedActors);
	}

	FHitResult Hit;

	// 벽/바닥 등 환경은 Overlap 이벤트를 생성하지 않는 경우가 많아 이동 구간을 트레이스로 검사
	if (GetWorld()->LineTraceSingleByChannel(Hit, LastTraceLocation, CurrentLocation, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = Hit.GetActor();

		if (!ShouldIgnoreActor(HitActor))
		{
			if (APawn* HitPawn = Cast<APawn>(HitActor))
			{
				HandleHitPawn(HitPawn, Hit.ImpactPoint);
			}
			else
			{
				BP_OnProjectileImpact(HitActor, Hit.ImpactPoint, EWarriorHitResultType::Invalid);
				Destroy();
				return;
			}
		}
	}

	LastTraceLocation = CurrentLocation;
}

void AWarriorProjectileBase::OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bLaunched || ShouldIgnoreActor(OtherActor))
	{
		return;
	}

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		HandleHitPawn(HitPawn, bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation());
	}
}

bool AWarriorProjectileBase::ShouldIgnoreActor(AActor* OtherActor) const
{
	if (!OtherActor || OtherActor == this || OtherActor->IsA<AWarriorProjectileBase>())
	{
		return true;
	}

	APawn* InstigatorPawn = GetInstigator();

	if (!InstigatorPawn)
	{
		return false;
	}

	return OtherActor == InstigatorPawn
		|| OtherActor->GetOwner() == InstigatorPawn
		|| OtherActor->GetAttachParentActor() == InstigatorPawn;
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
