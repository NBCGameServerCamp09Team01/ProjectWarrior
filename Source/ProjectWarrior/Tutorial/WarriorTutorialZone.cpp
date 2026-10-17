// Fill out your copyright notice in the Description page of Project Settings.

#include "WarriorTutorialZone.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Components/UI/PawnUIComponent.h"
#include "ProjectWarrior/Controllers/WarriorAIController.h"

AWarriorTutorialZone::AWarriorTutorialZone()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlayerStartPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("PlayerStartPoint"));
	PlayerStartPoint->SetupAttachment(SceneRoot);

	GoalVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("GoalVolume"));
	GoalVolume->SetupAttachment(SceneRoot);
	GoalVolume->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	GoalVolume->SetCollisionProfileName(TEXT("Trigger"));
	GoalVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleGoalVolumeBeginOverlap);

	//엔진 기본 원기둥을 얇게 눌러 바닥 원판으로 쓴다. 색은 기본 머티리얼의 Color 파라미터
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	GoalMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalMarker"));
	GoalMarker->SetupAttachment(GoalVolume);
	GoalMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GoalMarker->SetCastShadow(false);
	GoalMarker->SetHiddenInGame(true);
	if (CylinderFinder.Succeeded())
	{
		GoalMarker->SetStaticMesh(CylinderFinder.Object);
	}
	if (ShapeMaterialFinder.Succeeded())
	{
		GoalMarker->SetMaterial(0, ShapeMaterialFinder.Object);
	}
}

void AWarriorTutorialZone::SetGoalMarkerVisible(bool bInVisible)
{
	if (bInVisible)
	{
		//원기둥은 지름·높이 100cm. 도착 볼륨 바닥에 2cm 두께로 깐다
		const FVector Extent = GoalVolume->GetUnscaledBoxExtent();
		GoalMarker->SetRelativeLocation(FVector(0.f, 0.f, -Extent.Z + 1.f));
		GoalMarker->SetRelativeScale3D(FVector(Extent.X / 50.f, Extent.Y / 50.f, 0.02f));

		if (!GoalMarkerMaterial)
		{
			GoalMarkerMaterial = GoalMarker->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (GoalMarkerMaterial)
		{
			GoalMarkerMaterial->SetVectorParameterValue(TEXT("Color"), GoalMarkerColor);
		}
	}
	GoalMarker->SetHiddenInGame(!bInVisible);
}

void AWarriorTutorialZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//레벨을 떠날 때는 월드가 적을 함께 치운다. 구역만 지워질 때(Destroyed)만 직접 치운다
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		DeactivateZone();
	}
	else if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : RespawnTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AWarriorTutorialZone::ActivateZone(APawn* InPlayerPawn)
{
	PlayerPawn = InPlayerPawn;
	bZoneActive = true;

	if (bTeleportPlayer && InPlayerPawn)
	{
		const FTransform StartTransform = PlayerStartPoint->GetComponentTransform();
		InPlayerPawn->TeleportTo(StartTransform.GetLocation(), StartTransform.Rotator());
		if (AController* PlayerController = InPlayerPawn->GetController())
		{
			PlayerController->SetControlRotation(StartTransform.Rotator());
		}
	}

	SpawnedEnemies.SetNum(Enemies.Num());
	DyingEnemies.SetNum(Enemies.Num());
	RespawnTimers.SetNum(Enemies.Num());
	for (int32 Index = 0; Index < Enemies.Num(); ++Index)
	{
		//같은 구역의 다음 단계면 살아 있는 상대를 그대로 쓴다
		if (!SpawnedEnemies[Index].IsValid())
		{
			SpawnEnemy(Index);
		}
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Zone %s activated. enemies=%d"), *ZoneId.ToString(), Enemies.Num());
}

void AWarriorTutorialZone::DeactivateZone()
{
	if (!bZoneActive && SpawnedEnemies.IsEmpty())
	{
		return;
	}
	bZoneActive = false;
	SetGoalMarkerVisible(false);

	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : RespawnTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}

	for (const TWeakObjectPtr<AWarriorAICharacter>& Enemy : SpawnedEnemies)
	{
		if (AWarriorAICharacter* EnemyActor = Enemy.Get())
		{
			EnemyActor->OnCharacterDied.RemoveDynamic(this, &ThisClass::HandleEnemyDied);
			EnemyActor->Destroy();
		}
	}
	for (const TWeakObjectPtr<AWarriorAICharacter>& Body : DyingEnemies)
	{
		if (AWarriorAICharacter* BodyActor = Body.Get())
		{
			BodyActor->Destroy();
		}
	}
	SpawnedEnemies.Reset();
	DyingEnemies.Reset();
	RespawnTimers.Reset();
}

void AWarriorTutorialZone::SpawnEnemy(int32 InSpawnIndex)
{
	if (!bZoneActive || !Enemies.IsValidIndex(InSpawnIndex) || !Enemies[InSpawnIndex].EnemyClass)
	{
		return;
	}

	//사망 연출이 멈춰 몸이 남아 있으면 치운다(같은 자리에 새로 부른다)
	if (AWarriorAICharacter* Body = DyingEnemies.IsValidIndex(InSpawnIndex) ? DyingEnemies[InSpawnIndex].Get() : nullptr)
	{
		Body->Destroy();
	}

	const FWarriorTutorialSpawn& Spawn = Enemies[InSpawnIndex];
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform = Spawn.RelativeTransform * GetActorTransform();
	AWarriorAICharacter* Enemy = GetWorld()->SpawnActor<AWarriorAICharacter>(Spawn.EnemyClass, SpawnTransform, SpawnParameters);
	if (!Enemy)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] Zone %s failed to spawn %s."), *ZoneId.ToString(), *GetNameSafe(Spawn.EnemyClass));
		return;
	}

	SpawnedEnemies[InSpawnIndex] = Enemy;
	Enemy->OnCharacterDied.AddUniqueDynamic(this, &ThisClass::HandleEnemyDied);
	ApplySpawnSettings(Enemy, InSpawnIndex);

	//상대가 받는 게임플레이 이벤트를 모두 듣는다(무너짐 등). 빈 필터 = 모든 이벤트. 상대가 사라지면 ASC와 함께 풀린다
	if (UWarriorAbilitySystemComponent* ASC = Enemy->GetWarriorAbilitySystemComponent())
	{
		ASC->AddGameplayEventTagContainerDelegate(FGameplayTagContainer(),
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleEnemyGameplayEvent));
		//Death_Normal·Death_Knockback 모두 부모 태그 개수로 잡힌다
		ASC->RegisterGameplayTagEvent(WarriorGameplayTags::Shared_Status_Death, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleEnemyDeathTagChanged, TWeakObjectPtr<AWarriorAICharacter>(Enemy), InSpawnIndex);
	}

	if (AWarriorAIController* AIController = Cast<AWarriorAIController>(Enemy->GetController()))
	{
		if (Spawn.bStationary)
		{
			AIController->SetBehaviorSuppressed(true);
		}
		else
		{
			//웨이브 스포너와 같이 플레이어를 첫 대상으로 준다(AWarriorWaveSpawner::AssignInitialTarget)
			AIController->SetInitialTarget(PlayerPawn.Get());
		}
	}
}

void AWarriorTutorialZone::ApplySpawnSettings(AWarriorAICharacter* InEnemy, int32 InSpawnIndex) const
{
	UWarriorAbilitySystemComponent* ASC = InEnemy ? InEnemy->GetWarriorAbilitySystemComponent() : nullptr;
	if (!ASC || !Enemies.IsValidIndex(InSpawnIndex))
	{
		return;
	}

	//시작 데이터(GE)가 AttackPower·체력을 덮어쓴 뒤에 바꾼다. 로드가 끝나면 Initializing 태그가 빠진다(AWarriorAICharacter::InitEnemyStartUpData)
	if (ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Initializing))
	{
		ASC->RegisterGameplayTagEvent(WarriorGameplayTags::Shared_Status_Initializing, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleEnemyInitialized, TWeakObjectPtr<AWarriorAICharacter>(InEnemy), InSpawnIndex);
		return;
	}

	const FWarriorTutorialSpawn& Spawn = Enemies[InSpawnIndex];
	if (Spawn.bHarmless)
	{
		ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetAttackPowerAttribute(), 0.f);
	}
	if (Spawn.HealthRatio < 1.f)
	{
		const float MaxHealth = ASC->GetNumericAttribute(UWarriorAttributeSet::GetMaxHealthAttribute());
		ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetCurrentHealthAttribute(), FMath::Max(MaxHealth * Spawn.HealthRatio, 1.f));
		//체력바는 GE 실행 때만 갱신되므로 직접 알린다(UWarriorAttributeSet::PostGameplayEffectExecute)
		if (UPawnUIComponent* PawnUIComponent = InEnemy->GetPawnUIComponent())
		{
			PawnUIComponent->OnCurrentHealthChanged.Broadcast(Spawn.HealthRatio);
		}
	}
	if (Spawn.BalanceRatio < 1.f)
	{
		const float MaxBalance = ASC->GetNumericAttribute(UWarriorAttributeSet::GetMaxBalanceAttribute());
		ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetCurrentBalanceAttribute(), MaxBalance * Spawn.BalanceRatio);
	}
}

void AWarriorTutorialZone::HandleEnemyInitialized(const FGameplayTag InTag, int32 InNewCount, TWeakObjectPtr<AWarriorAICharacter> InEnemy, int32 InSpawnIndex) const
{
	if (InNewCount > 0)
	{
		return;
	}
	ApplySpawnSettings(InEnemy.Get(), InSpawnIndex);
}

void AWarriorTutorialZone::HandleEnemyGameplayEvent(FGameplayTag InEventTag, const FGameplayEventData* InPayload)
{
	if (bZoneActive)
	{
		OnEnemyEvent.Broadcast(this, InEventTag);
	}
}

void AWarriorTutorialZone::HandleEnemyDied(AWarriorBaseCharacter* DeadCharacter)
{
	const int32 SpawnIndex = SpawnedEnemies.IndexOfByPredicate([DeadCharacter](const TWeakObjectPtr<AWarriorAICharacter>& Enemy)
	{
		return Enemy.Get() == DeadCharacter;
	});
	if (SpawnIndex == INDEX_NONE || !bZoneActive || !Enemies[SpawnIndex].bRespawnWhenDead)
	{
		return;
	}

	//죽은 몸은 적 쪽 사망 처리가 치운다. 자리를 비우고 새로 부른다(그때까지 남아 있으면 SpawnEnemy가 치운다)
	DyingEnemies[SpawnIndex] = SpawnedEnemies[SpawnIndex];
	SpawnedEnemies[SpawnIndex].Reset();
	GetWorld()->GetTimerManager().SetTimer(RespawnTimers[SpawnIndex],
		FTimerDelegate::CreateUObject(this, &ThisClass::SpawnEnemy, SpawnIndex), FMath::Max(RespawnDelay, 0.01f), false);
}

void AWarriorTutorialZone::HandleEnemyDeathTagChanged(const FGameplayTag InTag, int32 InNewCount, TWeakObjectPtr<AWarriorAICharacter> InEnemy, int32 InSpawnIndex)
{
	if (InNewCount <= 0 || !bZoneActive)
	{
		return;
	}

	//평소에는 사망 연출 끝의 OnCharacterDied가 처리한다. DeathFallbackSeconds 안에 오지 않으면 여기서 처리한다
	//(이미 처리했으면 SpawnedEnemies에서 빠져 있어 HandleEnemyDied가 그냥 돌아간다)
	FTimerHandle FallbackTimer;
	GetWorld()->GetTimerManager().SetTimer(FallbackTimer, FTimerDelegate::CreateWeakLambda(this, [this, InEnemy, InSpawnIndex]()
	{
		if (!bZoneActive || !SpawnedEnemies.IsValidIndex(InSpawnIndex) || !Enemies[InSpawnIndex].bRespawnWhenDead)
		{
			return;
		}
		if (AWarriorAICharacter* Enemy = InEnemy.Get())
		{
			HandleEnemyDied(Enemy);
		}
		//몸이 OnCharacterDied 없이 사라졌다. 자리가 빈 채로 남지 않게 다시 부른다
		else if (!SpawnedEnemies[InSpawnIndex].IsValid() && !GetWorld()->GetTimerManager().IsTimerActive(RespawnTimers[InSpawnIndex]))
		{
			SpawnEnemy(InSpawnIndex);
		}
	}), FMath::Max(DeathFallbackSeconds, 0.01f), false);
}

void AWarriorTutorialZone::HandleGoalVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bZoneActive && OtherActor && OtherActor == PlayerPawn.Get())
	{
		OnZoneReached.Broadcast(this);
	}
}
