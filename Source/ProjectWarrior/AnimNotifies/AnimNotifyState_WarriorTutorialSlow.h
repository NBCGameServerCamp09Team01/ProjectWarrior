// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WarriorTutorialSlow.generated.h"

/**
 * 튜토리얼 전용 공격 몽타주의 공격 타이밍 구간(몽타주 타임라인에서 "Warrior Tutorial Slow").
 * 지금 단계가 힌트를 보인 상태면(AWarriorTutorialGameMode::IsSlowHintActive) 구간 동안 이 캐릭터만 느리게 한다.
 * 전역 시간 배율은 블레이드·전술 모드·인벤토리 휠이 끝날 때 1.0으로 덮어쓰므로 쓰지 않는다(계획서 2장 G).
 * 튜토리얼이 아닌 레벨에서는 아무것도 하지 않는다.
 */
UCLASS(meta = (DisplayName = "Warrior Tutorial Slow"))
class PROJECTWARRIOR_API UAnimNotifyState_WarriorTutorialSlow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	//~ Begin UAnimNotifyState Interface.
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	//~ End UAnimNotifyState Interface

	//구간 동안의 이 캐릭터 시간 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SlowDilation = 0.3f;
};
