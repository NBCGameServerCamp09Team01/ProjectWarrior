// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_OtherStrafers.generated.h"

/**
 * 쿼리하는 AI와 같은 대상(블랙보드 TargetActor)을 노리는 다른 AI들의 위치.
 * AI마다 현재 위치와, 정해 둔 목표 스트레이프 위치(블랙보드 StrafeLocation)를 둘 다 넣는다.
 * Distance 테스트에서 이 컨텍스트로부터 먼 점에 점수를 주면 AI들이 같은 자리로 몰리지 않는다.
 */
UCLASS(meta = (DisplayName = "Other Strafers"))
class PROJECTWARRIOR_API UEnvQueryContext_OtherStrafers : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Context")
	FName TargetActorKeyName = FName("TargetActor");

	UPROPERTY(EditDefaultsOnly, Category = "Context")
	FName StrafeLocationKeyName = FName("StrafeLocation");
};
