// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WarriorFaceTarget.generated.h"

/**
 * 구간 동안 AI가 대상 쪽으로 회전하는 노티파이 스테이트 (몽타주 타임라인에서 "Warrior Face Target").
 * 연타 공격의 각 타격 준비 동작에 배치하고, 휘두르기 직전에 끝나게 두면 그 뒤로는 방향이 고정되어 옆으로 피할 수 있다.
 * 대상: AI 컨트롤러 블랙보드의 TargetActor, 없으면 Focus 액터
 */
UCLASS(meta = (DisplayName = "Warrior Face Target"))
class PROJECTWARRIOR_API UAnimNotifyState_WarriorFaceTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_WarriorFaceTarget();

	//~ Begin UAnimNotifyState Interface.
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	//~ End UAnimNotifyState Interface

	// 회전 속도 (도/초, 0 = 즉시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify", meta = (ClampMin = "0.0"))
	float RotationSpeed = 360.f;
};
