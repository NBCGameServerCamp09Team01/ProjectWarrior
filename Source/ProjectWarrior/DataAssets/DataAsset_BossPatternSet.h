// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DataAsset_BossPatternSet.generated.h"

/** 보스 패턴 1개의 사용 조건. UBossPatternComponent가 조건을 만족하는 패턴 중 가중치 랜덤으로 선택한다. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorBossPatternData
{
	GENERATED_BODY()

	// 발동할 어빌리티 태그. 보스 StartUpData로 부여한 어빌리티의 AbilityTags와 일치해야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (Categories = "AI.Ability.Boss"))
	FGameplayTag AbilityTag;

	// 사용 가능한 대상 거리 (UBossPatternComponent의 거리 기준 설정을 따름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Condition", meta = (ClampMin = "0.0"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Condition", meta = (ClampMin = "0.0"))
	float MaxRange = 300.f;

	// 보스 정면과 대상 방향 사이 각도(0 = 정면, 180 = 정후방). 후방 패턴은 MinAngle을 높임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Condition", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MinAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Condition", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAngle = 180.f;

	// 이 페이즈부터 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Condition", meta = (ClampMin = "1"))
	int32 MinPhase = 1;

	// 이 페이즈까지 사용 (0 = 제한 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Condition", meta = (ClampMin = "0"))
	int32 MaxPhase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Selection", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// 발동 후 다시 선택될 때까지의 시간 (어빌리티 자체 쿨다운과 별개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Selection", meta = (ClampMin = "0.0"))
	float Cooldown = 0.f;

	// false면 직전에 사용한 패턴은 후보에서 제외
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Selection")
	bool bAllowRepeat = false;

	bool IsUsableInPhase(int32 InPhase) const;
};

/**
 * 보스 1종의 패턴 목록
 */
UCLASS()
class PROJECTWARRIOR_API UDataAsset_BossPatternSet : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FWarriorBossPatternData>& GetPatterns() const { return Patterns; }

	const FWarriorBossPatternData* FindPattern(const FGameplayTag& InAbilityTag) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Pattern", meta = (TitleProperty = "AbilityTag"))
	TArray<FWarriorBossPatternData> Patterns;
};
