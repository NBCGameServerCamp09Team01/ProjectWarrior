// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAIController.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "BehaviorTree/BlackboardComponent.h"

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
}

void AWarriorAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
	{
		if(!BlackboardComponent->GetValueAsObject(FName("TargetActor")))
		{
			if (Stimulus.WasSuccessfullySensed() && Actor)
			{
				BlackboardComponent->SetValueAsObject(FName("TargetActor"), Actor);
			}
		}
	}
}
