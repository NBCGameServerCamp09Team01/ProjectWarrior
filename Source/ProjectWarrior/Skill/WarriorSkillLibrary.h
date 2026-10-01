#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectWarrior/Stats/WarriorStatTypes.h"
#include "WarriorSkillTypes.h"
#include "WarriorSkillLibrary.generated.h"

class UDataAsset_SkillTree;

/**
 * 스킬 평가에 쓰는 입력 값 모음.
 * 게임에서는 MakeEvalContext가 계정·통계 서브시스템에서 채우고, 테스트에서는 직접 채운다.
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorSkillEvalContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	int32 AccountLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	int32 StatPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FGameplayTagContainer UnlockedSkills;

	/** S1 누적 통계 (이번 세션) */
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Skill")
	FWarriorLifetimeStats Lifetime;
};

/**
 * 스킬 조건 평가와 해금 요청 (SK).
 * - 평가 함수는 입력(FWarriorSkillEvalContext)만 보고 계산한다. 상태를 저장하지 않는다.
 * - 해금은 계정(UWarriorAccountSubsystem::UnlockSkill)에 맡긴다. 스탯 포인트 차감·해금 기록은 계정이 한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorSkillLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ 평가
	/** 계정·통계 서브시스템에서 평가 입력을 채운다. 계정 서브시스템이 없으면 false (통계가 없으면 누적 0으로 둔다) */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill", meta = (WorldContext = "WorldContextObject"))
	static bool MakeEvalContext(const UObject* WorldContextObject, FWarriorSkillEvalContext& OutContext);

	/** 조건 하나의 현재 값·목표 값·충족 여부 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	static FWarriorSkillRequirementProgress EvaluateRequirement(const FWarriorSkillRequirement& Requirement, const FWarriorSkillEvalContext& Context);

	/** 스킬 하나의 상태와 조건별 진행도 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	static FWarriorSkillView EvaluateSkill(const FWarriorSkillDefinition& Definition, const FWarriorSkillEvalContext& Context);

	/** 스킬 목록 전체를 DA 순서대로 평가 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	static TArray<FWarriorSkillView> EvaluateSkillTree(const UDataAsset_SkillTree* SkillTree, const FWarriorSkillEvalContext& Context);

	/** 현재 계정·통계로 스킬 목록을 평가한다 (화면용) */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill", meta = (WorldContext = "WorldContextObject"))
	static TArray<FWarriorSkillView> BuildSkillViews(const UObject* WorldContextObject, const UDataAsset_SkillTree* SkillTree);

	//~ 해금
	/** 다시 평가해서 Unlockable일 때만 계정에 해금을 요청한다. 성공하면 true */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill", meta = (WorldContext = "WorldContextObject"))
	static bool TryUnlockSkill(const UObject* WorldContextObject, const UDataAsset_SkillTree* SkillTree, FGameplayTag SkillTag);
};
