// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "AnimNotifyState_WarriorBlockRule.generated.h"

/**
 * 구간 동안 무기 공격의 막기 규칙을 바꾸는 노티파이 스테이트 (몽타주 타임라인에서 "Warrior Block Rule").
 * 무기 충돌 노티파이 구간을 덮도록 배치한다. 끝나면 Blockable로 돌아감 (충돌을 꺼도 돌아감)
 * 예: 보스 분노 연타의 마지막 타격 = PerfectParryOnly + bStaggerOnBlocked
 */
UCLASS(meta = (DisplayName = "Warrior Block Rule"))
class PROJECTWARRIOR_API UAnimNotifyState_WarriorBlockRule : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	//~ Begin UAnimNotifyState Interface.
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	//~ End UAnimNotifyState Interface

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify")
	EWarriorBlockRule BlockRule = EWarriorBlockRule::PerfectParryOnly;

	// 이 구간의 공격이 막히면 공격자에게 Shared.Event.Stagger를 보냄
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify")
	bool bStaggerOnBlocked = true;
};
