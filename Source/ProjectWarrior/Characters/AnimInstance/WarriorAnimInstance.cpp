// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAnimInstance.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"

void UWarriorAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UWarriorAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
}

void UWarriorAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// ALS가 계산한 발 IK 값을 애님 그래프가 읽기 전에 덮어씀
	const bool bRootMotionMontage = bDisableFootIKDuringRootMotionMontage && GetRootMotionMontageInstance();

	// 쓰러진 시체에 발 IK·골반 보정이 들어가면 다리가 꺾이거나 몸이 들뜸
	// (IsActorDead는 null을 죽은 것으로 보므로, 애님 에디터 미리보기처럼 폰이 없을 때는 제외)
	APawn* OwningPawn = TryGetPawnOwner();
	const bool bDead = bDisableFootIKWhenDead && OwningPawn && UWarriorFunctionLibrary::IsActorDead(OwningPawn);

	if (bRootMotionMontage || bDead)
	{
		SuppressFootIK();
	}
}

void UWarriorAnimInstance::SuppressFootIK()
{
	// 잠금 해제. 몽타주가 끝나면 ALS가 FootLock 커브로 그 시점의 발 위치를 새로 잠금
	FootIKValues.FootLock_L_Alpha = 0.0f;
	FootIKValues.FootLock_R_Alpha = 0.0f;
	FootIKValues.UseFootLockCurve_L = false;
	FootIKValues.UseFootLockCurve_R = false;

	// 바닥 맞춤 오프셋 제거. 몽타주가 끝나면 ALS가 0에서부터 다시 보간
	FootIKValues.FootOffset_L_Location = FVector::ZeroVector;
	FootIKValues.FootOffset_R_Location = FVector::ZeroVector;
	FootIKValues.FootOffset_L_Rotation = FRotator::ZeroRotator;
	FootIKValues.FootOffset_R_Rotation = FRotator::ZeroRotator;

	FootIKValues.PelvisOffset = FVector::ZeroVector;
	FootIKValues.PelvisAlpha = 0.0f;
}
