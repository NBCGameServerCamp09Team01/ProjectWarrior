#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectWarrior/Stats/WarriorStatTypes.h"
#include "WarriorSkillTypes.h"
#include "WarriorSkillLibrary.generated.h"

class UDataAsset_SkillTree;
struct FWarriorServerSkillView;
struct FWarriorServerSkillRequirement;

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
 * 스킬 조건 평가 (SK).
 * - 평가 함수는 입력(FWarriorSkillEvalContext)만 보고 계산한다. 상태를 저장하지 않는다.
 * - 로그인 상태에서는 서버가 판정한 진행도(K2, UWarriorSkillUnlockSubsystem)로 상태·진행도를 덮어쓴다(ApplyServerProgress).
 *   DA는 이름·설명·아이콘·조건 문구를 주고, 판정 값은 서버가 준다(S4, 루트 docs/contracts/skill-api.md).
 * - 해금은 서버만 한다(UWarriorSkillUnlockSubsystem). 로컬 해금 길은 없다.
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

	//~ 서버 진행도
	/**
	 * 서버가 판정한 스킬 상태·진행도로 화면 값을 덮어쓴다. DA의 조건마다 같은 종류의 서버 조건 값을 쓴다.
	 * 서버에 없는 스킬(DA에만 있음)은 해금할 수 없으므로 잠금으로 둔다. 서버가 판정하지 않는 DA 조건은 로컬 값 그대로다
	 */
	static void ApplyServerProgress(TArray<FWarriorSkillView>& Views, const TArray<FWarriorServerSkillView>& ServerSkills);

	/** 서버 조건 목록 하나를 화면 값 하나에 덮어쓴다(진행도·선행 스킬). 상태는 바꾸지 않는다 */
	static void ApplyServerRequirements(FWarriorSkillView& View, const TArray<FWarriorServerSkillRequirement>& Requirements);
};
