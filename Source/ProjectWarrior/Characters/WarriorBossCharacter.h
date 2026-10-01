// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorAICharacter.h"
#include "WarriorBossCharacter.generated.h"

class UBossPatternComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorBossPhaseChanged, int32, OldPhase, int32, NewPhase);

/**
 * 보스 전용 AI 캐릭터. 스폰·사망·보상은 AWarriorAICharacter 흐름을 그대로 사용하고,
 * 패턴 선택(UBossPatternComponent)과 페이즈 상태를 추가로 가진다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorBossCharacter : public AWarriorAICharacter
{
	GENERATED_BODY()

public:
	AWarriorBossCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	int32 GetMaxPhase() const { return MaxPhase; }

	// 1 ~ MaxPhase로 보정해서 적용. 값이 바뀌면 OnBossPhaseChanged 방송
	UFUNCTION(BlueprintCallable, Category = "Warrior|Boss")
	void SetPhase(int32 NewPhase);

	// AI.Status.Boss.Blocking 보유 여부
	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	bool IsBossBlocking() const;

	// AI.Status.Boss.SuperArmor 보유 여부
	UFUNCTION(BlueprintPure, Category = "Warrior|Boss")
	bool HasSuperArmor() const;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Boss")
	FOnWarriorBossPhaseChanged OnBossPhaseChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss", meta = (AllowPrivateAccess = "true"))
	UBossPatternComponent* BossPatternComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "1"))
	int32 MaxPhase = 2;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Boss")
	int32 CurrentPhase = 1;

public:
	FORCEINLINE UBossPatternComponent* GetBossPatternComponent() const { return BossPatternComponent; }
};
