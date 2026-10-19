#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WarriorSkillTypes.generated.h"

/**
 * 스킬 해금 데이터 모델 (SK).
 * - 스킬 하나 = 해금 조건 목록(모두 충족) + 선행 스킬 + 스탯 포인트 비용.
 * - 조건 값은 대부분 S1 통계 누적(Lifetime)과 계정 레벨에서 읽는다. 평가는 UWarriorSkillLibrary가 한다 (SK-2).
 * - 해금 기록은 계정(UWarriorAccountSubsystem)에만 둔다. 스킬 쪽은 상태를 따로 저장하지 않는다.
 * - 화면에는 정의 + 상태 + 조건별 진행도(FWarriorSkillView)를 넘긴다. 디자인이 바뀌어도 이 구조는 그대로 쓴다.
 */

/** 해금 조건 종류. 새 조건은 여기에 하나 추가하고 평가 함수에 case를 하나 더한다. */
UENUM(BlueprintType)
enum class EWarriorSkillRequirementType : uint8
{
	AccountLevel,		// 계정 레벨 N 이상
	TotalKills,			// 누적 처치 N 이상. Key에 적 종류(BP_SamuraiAI 등)를 넣으면 그 종류만
	KillsByDeathType,	// 사망 방식별 처치 N 이상. Key = Normal / Knockback / Finisher
	StagesCleared,		// 스테이지 클리어 N회 이상
	PlayTimeSeconds,	// 누적 플레이 시간 N초 이상
	MaxWaveReached,		// 최대 도달 웨이브 N 이상
	BestClearTime,		// 최고 클리어 시간 N초 이하 (기록이 없으면 미충족)
	TotalDamage,		// 누적 데미지 N 이상 (데미지 기록 연결 후 사용)
	MaxDamage,			// 한 방 최대 데미지 N 이상 (데미지 기록 연결 후 사용)
	ExtraStat			// 추가 통계(StatTag, Key) 합계 N 이상
};

/** 화면에 보이는 스킬 상태 */
UENUM(BlueprintType)
enum class EWarriorSkillState : uint8
{
	Locked,			// 조건 미충족 또는 선행 스킬 미해금
	NeedPoints,		// 조건·선행은 충족, 스탯 포인트 부족
	Unlockable,		// 해금 버튼 활성
	Unlocked		// 해금됨
};

/** 해금 조건 하나 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorSkillRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	EWarriorSkillRequirementType Type = EWarriorSkillRequirementType::AccountLevel;

	/** 목표 값. BestClearTime만 "이하", 나머지는 "이상" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill", meta = (ClampMin = "0"))
	double TargetValue = 1.0;

	/** 세부 키. TotalKills: 적 종류, KillsByDeathType: 사망 방식, ExtraStat: 세부 키. 비우면 전체 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	FName Key = NAME_None;

	/** ExtraStat일 때 읽을 통계 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill", meta = (Categories = "Stat", EditCondition = "Type == EWarriorSkillRequirementType::ExtraStat", EditConditionHides))
	FGameplayTag StatTag;

	/** 화면 문구. 비우면 타입별 기본 문구를 쓴다 ("누적 처치 30 이상") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	FText DisplayText;

	/** 목표 값이 상한(이하)인 조건이면 true */
	bool IsUpperBound() const { return Type == EWarriorSkillRequirementType::BestClearTime; }

	/** DisplayText가 비어 있으면 타입별 기본 문구를 만든다 */
	FText GetDisplayText() const;
};

/** 스킬 하나의 정의 (DA_SkillTree에 나열) */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorSkillDefinition
{
	GENERATED_BODY()

	/** 해금 기록 키. 계정의 UnlockedSkills에 이 태그로 남는다. 정한 뒤에는 바꾸지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill", meta = (Categories = "Account.Skill"))
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill", meta = (MultiLine = "true"))
	FText Description;

	/** 틀 단계에서는 비워도 된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	TSoftObjectPtr<UTexture2D> SoftIcon;

	/** 해금에 쓰는 스탯 포인트. 조건을 모두 충족한 뒤에 차감한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill", meta = (ClampMin = "0"))
	int32 StatPointCost = 1;

	/** 해금 조건. 모두 충족해야 한다 (AND) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	TArray<FWarriorSkillRequirement> Requirements;

	/** 먼저 해금해야 하는 스킬. 트리 선을 그릴 때도 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill", meta = (Categories = "Account.Skill"))
	TArray<FGameplayTag> Prerequisites;

	/** 갈래 이름 (검술 / 처형 / 생존). 디자인 단계에서 트리 배치에 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Skill")
	FName Branch = NAME_None;
};

/** 조건 하나의 진행도 (화면 표시용) */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorSkillRequirementProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	double Current = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	double Target = 0.0;

	/** 이하 조건이면 true (BestClearTime) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	bool bUpperBound = false;

	/** 기록이 있는지. BestClearTime처럼 기록이 없을 수 있는 조건에서 false */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	bool bHasValue = true;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	bool bMet = false;

	/** 진행 바용 0~1. 이하 조건은 충족 여부만 (0 또는 1) */
	double GetRatio() const;
};

/** 스킬 하나의 화면 표시 값 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorSkillView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FWarriorSkillDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	EWarriorSkillState State = EWarriorSkillState::Locked;

	/** Definition.Requirements와 같은 순서 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	TArray<FWarriorSkillRequirementProgress> Requirements;

	/** 아직 해금되지 않은 선행 스킬 */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	TArray<FGameplayTag> MissingPrerequisites;

	bool IsUnlocked() const { return State == EWarriorSkillState::Unlocked; }
	bool CanUnlock() const { return State == EWarriorSkillState::Unlockable; }
};
