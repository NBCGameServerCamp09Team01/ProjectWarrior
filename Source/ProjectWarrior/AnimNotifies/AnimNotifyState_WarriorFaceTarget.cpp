// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotifyState_WarriorFaceTarget.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// AI 컨트롤러 블랙보드의 TargetActor (AWarriorAIController::OnPerceptionUpdated가 채움), 없으면 Focus 액터
	AActor* FindFaceTarget(const AAIController* AIController)
	{
		if (const UBlackboardComponent* BlackboardComponent = AIController->GetBlackboardComponent())
		{
			if (AActor* TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetActor"))))
			{
				return TargetActor;
			}
		}

		return AIController->GetFocusActor();
	}
}

UAnimNotifyState_WarriorFaceTarget::UAnimNotifyState_WarriorFaceTarget()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(80, 200, 255);
#endif
}

FString UAnimNotifyState_WarriorFaceTarget::GetNotifyName_Implementation() const
{
	return RotationSpeed > 0.f
		? FString::Printf(TEXT("Face Target (%.0f/s)"), RotationSpeed)
		: TEXT("Face Target (Snap)");
}

void UAnimNotifyState_WarriorFaceTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	APawn* OwnerPawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
	const AAIController* AIController = OwnerPawn ? Cast<AAIController>(OwnerPawn->GetController()) : nullptr;

	// 애니메이션 에디터 미리 보기, 플레이어 캐릭터에서는 아무것도 하지 않음
	if (!AIController || !OwnerPawn->HasAuthority())
	{
		return;
	}

	const AActor* TargetActor = FindFaceTarget(AIController);

	if (!TargetActor)
	{
		return;
	}

	const FVector ToTarget = (TargetActor->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal2D();

	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator CurrentRotation(0.f, OwnerPawn->GetActorRotation().Yaw, 0.f);
	const FRotator DesiredRotation = ToTarget.Rotation();

	const FRotator NewRotation = RotationSpeed > 0.f
		? FMath::RInterpConstantTo(CurrentRotation, DesiredRotation, FrameDeltaTime, RotationSpeed)
		: DesiredRotation;

	UWarriorFunctionLibrary::SetPawnFacingRotation(OwnerPawn, NewRotation);
}
