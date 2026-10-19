// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "BossPatternComponent.generated.h"

class UDataAsset_BossPatternSet;

/**
 * 대상과의 거리·각도, 보스 페이즈, 패턴 쿨다운, 어빌리티 발동 가능 여부로 다음 패턴을 고른다.
 * BT에서는 UBTTask_SelectBossPattern -> UBTTask_ActivateBossPattern 순으로 사용.
 */
UCLASS()
class PROJECTWARRIOR_API UBossPatternComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	// 조건을 만족하는 패턴 중 가중치 랜덤으로 1개 선택. 후보가 없으면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|BossPattern")
	bool SelectPattern(AActor* TargetActor, FGameplayTag& OutPatternTag);

	// 패턴 어빌리티 발동 성공 시 호출. 패턴 쿨다운 시작, 직전 패턴 기록
	UFUNCTION(BlueprintCallable, Category = "Warrior|BossPattern")
	void NotifyPatternActivated(FGameplayTag PatternTag);

	UFUNCTION(BlueprintPure, Category = "Warrior|BossPattern")
	bool IsPatternOnCooldown(FGameplayTag PatternTag) const;

	UFUNCTION(BlueprintPure, Category = "Warrior|BossPattern")
	float GetPatternRemainingCooldown(FGameplayTag PatternTag) const;

	UFUNCTION(BlueprintCallable, Category = "Warrior|BossPattern")
	void ResetAllPatternCooldowns();

	UFUNCTION(BlueprintPure, Category = "Warrior|BossPattern")
	FGameplayTag GetLastPatternTag() const { return LastPatternTag; }

	// 패턴 조건 판단에 쓰는 거리(수평)와 정면 기준 각도(0~180)
	UFUNCTION(BlueprintPure, Category = "Warrior|BossPattern")
	bool GetTargetDistanceAndAngle(AActor* TargetActor, float& OutDistance, float& OutAngle) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "BossPattern")
	TObjectPtr<UDataAsset_BossPatternSet> PatternSet;

	// true: 두 캡슐 표면 사이 거리(보스 덩치와 무관하게 사거리 설정), false: 중심 사이 거리
	UPROPERTY(EditDefaultsOnly, Category = "BossPattern")
	bool bUseCapsuleSurfaceDistance = true;

	UPROPERTY(EditDefaultsOnly, Category = "BossPattern|Debug")
	bool bLogPatternSelection = false;

private:
	bool CanActivatePatternAbility(const FGameplayTag& PatternTag) const;
	int32 GetCurrentPhase() const;

	// 패턴 태그 -> 쿨다운이 끝나는 월드 시간
	TMap<FGameplayTag, double> PatternCooldownEndTimes;

	FGameplayTag LastPatternTag;
};
