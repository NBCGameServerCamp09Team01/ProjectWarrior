// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectWarrior/Tutorial/WarriorTutorialTypes.h"
#include "DataAsset_TutorialSteps.generated.h"

/**
 * 튜토리얼의 장·단계 목록(계획서 2장 B). 문구·순서·완료 조건·풀리는 어빌리티를 C++ 빌드 없이 고친다.
 * AWarriorTutorialGameMode가 진행하고, 메인메뉴의 장 고르기 팝업이 장 제목을 읽는다.
 */
UCLASS(BlueprintType)
class PROJECTWARRIOR_API UDataAsset_TutorialSteps : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FWarriorTutorialChapter>& GetChapters() const { return Chapters; }

	//없으면 nullptr
	const FWarriorTutorialStep* FindStep(int32 InChapterIndex, int32 InStepIndex) const;

	//모든 단계의 UnlockAbilityTags 합. 튜토리얼 시작 때 이것을 모두 막는다
	FGameplayTagContainer GetAllUnlockAbilityTags() const;

	//처음부터 이 단계까지(포함) 푼 어빌리티
	FGameplayTagContainer GetUnlockedAbilityTagsUpTo(int32 InChapterIndex, int32 InStepIndex) const;

	//튜토리얼 동안만 플레이어에게 붙이는 계정 스킬 태그(예: Account.Skill.DodgeMastery). 계정 사본·서버 값은 바꾸지 않는다(계획서 2장 E)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (Categories = "Account.Skill"))
	FGameplayTagContainer TemporarySkillTags;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (AllowPrivateAccess = "true", TitleProperty = "Title"))
	TArray<FWarriorTutorialChapter> Chapters;
};
