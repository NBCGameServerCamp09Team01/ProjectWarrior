#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WarriorSkillTypes.h"
#include "DataAsset_SkillTree.generated.h"

/**
 * 스킬 탭에 나오는 스킬 목록 (SK).
 * 배열 순서가 화면 표시 순서다. 트리 배치는 디자인 단계에서 Branch·Prerequisites로 그린다.
 */
UCLASS()
class PROJECTWARRIOR_API UDataAsset_SkillTree : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Skill", meta = (TitleProperty = "DisplayName"))
	TArray<FWarriorSkillDefinition> Skills;

	/** 태그로 스킬 정의를 찾는다. 없으면 nullptr */
	const FWarriorSkillDefinition* FindSkill(const FGameplayTag& InSkillTag) const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	bool GetSkill(FGameplayTag SkillTag, FWarriorSkillDefinition& OutDefinition) const;

#if WITH_EDITOR
	/** 빈 태그, 중복 태그, 목록에 없는 선행 스킬, 자기 자신·순환 선행을 잡는다 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
