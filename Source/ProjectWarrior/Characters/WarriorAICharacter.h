// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Stage/WarriorDifficultyScaleGate.h"
#include "ProjectWarrior/Stage/WarriorAffixCatalogDataAsset.h"
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

	float GetDifficultyHealthMultiplier() const { return DifficultyScaleGate.HealthMultiplier; }
	float GetDifficultyDamageMultiplier() const { return DifficultyScaleGate.DamageMultiplier; }

	// 수식어(서버 웨이브 구성의 적 줄 affixes). 웨이브 생성기가 SetDifficultyScale 바로 뒤에 부른다(졸개는 물려받지 않음).
	// DA_AffixCatalog에 없는 키는 빼고 경고한다. 시작 데이터·난이도 배율이 적용된 뒤에 한 번 자기 GE를 건다
	void SetAffixes(const TArray<FName>& InAffixIds);

	// 붙은 수식어 키(카탈로그에 있는 것만, 웨이브 순서)
	const TArray<FName>& GetAffixIds() const { return AffixIds; }

	// 붙은 수식어 수로 정한 등급(없음·챔피언·희귀)
	EWarriorAffixTier GetAffixTier() const { return UWarriorAffixCatalogDataAsset::GetTierForCount(AffixIds.Num()); }

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

	// 난이도 배율을 최대·현재 체력과 공격력 기본값에 곱한다. 언제 부를지는 DifficultyScaleGate가 정한다(한 번만)
	void ApplyDifficultyScale();

	// 배율과 시작 데이터 중 어느 쪽이 먼저 와도 둘 다 준비됐을 때 한 번만 곱하게 하는 판정
	FWarriorDifficultyScaleGate DifficultyScaleGate;

	// 시작 데이터가 적용됐고 수식어를 아직 걸지 않았으면 건다. 시작 데이터 콜백(난이도 배율 뒤)과 SetAffixes가 부른다
	void TryApplyAffixes();

	// 자기 GE·태그를 건다. 최대 체력이 늘면 현재 체력도 같은 비율로 올린다(시작 체력이 가득 찬 채로)
	void ApplyAffixes();

	UPROPERTY(VisibleInstanceOnly, Category = "Affix")
	TArray<FName> AffixIds;

	bool bAffixesApplied = false;

public:
	FORCEINLINE UAICombatComponent* GetAICombatComponent() const { return AICombatComponent; }
};
