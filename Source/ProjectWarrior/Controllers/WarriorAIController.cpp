// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAIController.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	TAutoConsoleVariable<int32> CVarStrafeDebug(
		TEXT("pw.Strafe.Debug"),
		0,
		TEXT("스트레이프 디버그 표시. 0: 끔, 1: AI마다 목표 스트레이프 위치(노랑), 도는 방향 화살표(초록 시계/주황 반시계), 반경 보정값 표시"),
		ECVF_Cheat);
}
#endif

AWarriorAIController::AWarriorAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>("PathFollowingComponent"))
{
	if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		//Debug::Print(TEXT("CrowdFollowingComponent valid"), FColor::Green);
	}

	AISenseConfig_Sight = CreateDefaultSubobject<UAISenseConfig_Sight>("SenseConfig_Sight");
	AISenseConfig_Sight->DetectionByAffiliation.bDetectEnemies = true;
	AISenseConfig_Sight->DetectionByAffiliation.bDetectFriendlies = false;
	AISenseConfig_Sight->DetectionByAffiliation.bDetectNeutrals = false;
	AISenseConfig_Sight->SightRadius = 800.f;
	AISenseConfig_Sight->LoseSightRadius = 1200.f;
	AISenseConfig_Sight->PeripheralVisionAngleDegrees = 60.f;
	AISenseConfig_Sight->SetMaxAge(5.f);
	AISenseConfig_Sight->AutoSuccessRangeFromLastSeenLocation = -1.f;

	AISenseConfig_Hearing = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("SenseConfig_Hearing"));
	AISenseConfig_Hearing->DetectionByAffiliation.bDetectEnemies = true;
	AISenseConfig_Hearing->DetectionByAffiliation.bDetectNeutrals = true;
	AISenseConfig_Hearing->DetectionByAffiliation.bDetectFriendlies = true;
	AISenseConfig_Hearing->HearingRange = 500.f;
	AISenseConfig_Hearing->SetMaxAge(3.f);

	AISenseConfig_Damage = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("SenseConfig_Damage"));
	AISenseConfig_Damage->SetMaxAge(3.f);

	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("PerceptionComponent");
	AIPerceptionComponent->ConfigureSense(*AISenseConfig_Sight);
	AIPerceptionComponent->ConfigureSense(*AISenseConfig_Hearing);
	AIPerceptionComponent->ConfigureSense(*AISenseConfig_Damage);
	AIPerceptionComponent->SetDominantSense(UAISenseConfig_Sight::StaticClass());
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::OnPerceptionUpdated);

	SetGenericTeamId(FGenericTeamId(1));
}

ETeamAttitude::Type AWarriorAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const APawn* PawnToCheck = Cast<const APawn>(&Other);

	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(PawnToCheck->GetController());

	if (OtherTeamAgent && OtherTeamAgent->GetGenericTeamId() < GetGenericTeamId())
	{
		return ETeamAttitude::Hostile;
	}

	return ETeamAttitude::Friendly;
}

void AWarriorAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		CrowdComp->SetCrowdSimulationState(bEnableDetourCrowdAvoidance ? ECrowdSimulationState::Enabled : ECrowdSimulationState::Disabled);

		switch (DetourCrowdAvoidanceQuality)
		{
		case 1: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);    break;
		case 2: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium); break;
		case 3: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);   break;
		case 4: CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);   break;
		default:
			break;
		}

		CrowdComp->SetAvoidanceGroup(1);
		CrowdComp->SetGroupsToAvoid(1);
		CrowdComp->SetCrowdCollisionQueryRange(CollisionQueryRange);
	}

	// AI마다 도는 방향과 선호 반경을 다르게 해서 스트레이프 위치가 한곳에 몰리지 않게 함
	StrafeOrbitDirection = FMath::RandBool() ? 1 : -1;
	StrafeRadiusOffset = FMath::FRandRange(-StrafeRadiusVariance, StrafeRadiusVariance);
	ScheduleStrafeOrbitFlip();
}

void AWarriorAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (CVarStrafeDebug.GetValueOnGameThread() > 0)
	{
		DrawStrafeDebug();
	}
#endif
}

void AWarriorAIController::FlipStrafeOrbitDirection()
{
	StrafeOrbitDirection = -StrafeOrbitDirection;
	ScheduleStrafeOrbitFlip();
}

void AWarriorAIController::ScheduleStrafeOrbitFlip()
{
	if (StrafeOrbitFlipInterval.Y <= 0.f)
	{
		return;
	}

	const float Interval = FMath::FRandRange(FMath::Max(StrafeOrbitFlipInterval.X, 0.1f), FMath::Max(StrafeOrbitFlipInterval.X, StrafeOrbitFlipInterval.Y));
	GetWorldTimerManager().SetTimer(StrafeOrbitFlipTimerHandle, this, &ThisClass::FlipStrafeOrbitDirection, Interval, false);
}

void AWarriorAIController::DrawStrafeDebug() const
{
#if ENABLE_DRAW_DEBUG
	const APawn* ControlledPawn = GetPawn();
	const UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!ControlledPawn || !BlackboardComponent)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const FVector PawnLocation = ControlledPawn->GetActorLocation();

	// 목표 스트레이프 위치
	if (BlackboardComponent->IsVectorValueSet(FName("StrafeLocation")))
	{
		const FVector StrafeLocation = BlackboardComponent->GetValueAsVector(FName("StrafeLocation"));
		DrawDebugLine(World, PawnLocation, StrafeLocation, FColor::Yellow, false, -1.f, 0, 1.5f);
		DrawDebugSphere(World, StrafeLocation, 25.f, 8, FColor::Yellow);
	}

	// 대상 주위를 도는 방향 (접선 화살표)
	if (const AActor* TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetActor"))))
	{
		const FVector FromTarget = (PawnLocation - TargetActor->GetActorLocation()).GetSafeNormal2D();
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, FromTarget) * StrafeOrbitDirection;
		DrawDebugDirectionalArrow(World, PawnLocation, PawnLocation + Tangent * 150.f, 40.f,
			StrafeOrbitDirection > 0 ? FColor::Green : FColor::Orange, false, -1.f, 0, 3.f);
		DrawDebugString(World, PawnLocation + FVector(0.f, 0.f, 140.f),
			FString::Printf(TEXT("%s  r%+.0f"), StrafeOrbitDirection > 0 ? TEXT("CW") : TEXT("CCW"), StrafeRadiusOffset),
			nullptr, FColor::White, 0.f, true);
	}
#endif
}

bool AWarriorAIController::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	const bool bRan = Super::RunBehaviorTree(BTAsset);

	if (bRan && PendingInitialTarget.IsValid())
	{
		TrySetTargetActor(PendingInitialTarget.Get());
		PendingInitialTarget.Reset();
	}

	return bRan;
}

void AWarriorAIController::SetInitialTarget(AActor* InTarget)
{
	if (!InTarget)
	{
		return;
	}

	// 스폰 직후에는 BT(블랙보드)가 아직 실행되지 않았을 수 있음
	if (!TrySetTargetActor(InTarget) && !GetBlackboardComponent())
	{
		PendingInitialTarget = InTarget;
	}
}

void AWarriorAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.WasSuccessfullySensed())
	{
		TrySetTargetActor(Actor);
	}
}

bool AWarriorAIController::TrySetTargetActor(AActor* InTarget)
{
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();

	if (!InTarget || !BlackboardComponent || BlackboardComponent->GetValueAsObject(FName("TargetActor")))
	{
		return false;
	}

	BlackboardComponent->SetValueAsObject(FName("TargetActor"), InTarget);
	return true;
}
