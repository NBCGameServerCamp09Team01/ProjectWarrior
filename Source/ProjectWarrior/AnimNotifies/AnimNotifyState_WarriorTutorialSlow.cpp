// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotifyState_WarriorTutorialSlow.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProjectWarrior/GameModes/WarriorTutorialGameMode.h"

FString UAnimNotifyState_WarriorTutorialSlow::GetNotifyName_Implementation() const
{
	return TEXT("Warrior Tutorial Slow");
}

void UAnimNotifyState_WarriorTutorialSlow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const AWarriorTutorialGameMode* TutorialGameMode = World ? World->GetAuthGameMode<AWarriorTutorialGameMode>() : nullptr;
	if (TutorialGameMode && TutorialGameMode->IsSlowHintActive())
	{
		Owner->CustomTimeDilation = SlowDilation;
	}
}

void UAnimNotifyState_WarriorTutorialSlow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	//이 구간이 바꾼 값일 때만 되돌린다(그 사이 전술 모드가 멈춘 배율 등은 건드리지 않는다)
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (Owner && FMath::IsNearlyEqual(Owner->CustomTimeDilation, SlowDilation))
	{
		Owner->CustomTimeDilation = 1.f;
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
