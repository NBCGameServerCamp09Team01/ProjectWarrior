// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "WarriorAICharacter.generated.h"

class UAICombatComponent;
class UAIUIComponent;
class UWidgetComponent;
struct FGameplayEventData;

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorAICharacter : public AWarriorBaseCharacter
{
	GENERATED_BODY()

public:
	AWarriorAICharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin PawnCombatInterface Interface.
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface Interface

	//~ Begin PawnUIInterface Interface.
	virtual UPawnUIComponent* GetPawnUIComponent() const override;
	virtual UAIUIComponent* GetAIUIComponent() const override;
	//~ End PawnUIInterface Interface

	// 사망 처리 시작(OnDeathAbilityStart)에서 호출됨. 사망 알림 후 AI 회전·이동을 멈춤
	virtual void OnCharacterDiedEvent_Implementation() override;

	// 사망 몽타주가 쓰러진 자세에 도달했는지(AI.Event.Death.PoseReached를 받았는지)
	bool HasReachedDeathPose() const;

	// UI(전술 모드 대상 목록 등)에 보이는 이름. 비어 있으면 BP 클래스 이름에서 만듦(BP_SamuraiAI → SamuraiAI)
	FText GetDisplayName() const;

	// 난이도 배율(M9, 서버 웨이브 구성의 적 줄 healthMultiplier·damageMultiplier). 웨이브 생성기·졸개 소환이 스폰 직후 부른다.
	// 시작 데이터(GE, 비동기 로드)가 적용된 뒤에 최대·현재 체력과 공격력 기본값에 한 번 곱한다(먼저 곱하면 시작 GE의 Override가 덮어씀)
	void SetDifficultyScale(float InHealthMultiplier, float InDamageMultiplier);

	float GetDifficultyHealthMultiplier() const { return DifficultyHealthMultiplier; }
	float GetDifficultyDamageMultiplier() const { return DifficultyDamageMultiplier; }

protected:
	virtual void BeginPlay() override;

	// 사망 GA의 몽타주가 쓰러진 자세에 닿을 때 오는 이벤트. 태그를 붙여 이후 공격 판정에서 제외되게 함
	void OnDeathPoseReached(const FGameplayEventData* Payload);

	//~ Begin APawn Interface.
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	UAICombatComponent* AICombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UAIUIComponent* AIUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* AIHealthWidgetComponent;

	// 빙의 시 ASC에 붙여 두는 상태 태그 (예: 처형 불가 Shared.Status.FinisherImmune). 보스는 기본으로 처형 불가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (Categories = "Shared.Status,AI.Status"))
	FGameplayTagContainer DefaultStatusTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText DisplayName;

private:
	void InitAIStartUpData();

	// 시작 데이터가 적용됐고 아직 곱하지 않았으면 난이도 배율을 곱한다(한 번만)
	void ApplyDifficultyScale();

	float DifficultyHealthMultiplier = 1.f;
	float DifficultyDamageMultiplier = 1.f;

	// 시작 데이터(GE) 적용이 끝났는가. 그 전에 받은 배율은 적용을 미룬다
	bool bStartUpDataApplied = false;

	bool bDifficultyScaleApplied = false;

public:
	FORCEINLINE UAICombatComponent* GetAICombatComponent() const { return AICombatComponent; }
};
