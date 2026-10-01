#include "DataAsset_SkillTree.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WarriorSkill"

const FWarriorSkillDefinition* UDataAsset_SkillTree::FindSkill(const FGameplayTag& InSkillTag) const
{
	if (!InSkillTag.IsValid())
	{
		return nullptr;
	}
	return Skills.FindByPredicate([&InSkillTag](const FWarriorSkillDefinition& Candidate)
	{
		return Candidate.SkillTag == InSkillTag;
	});
}

bool UDataAsset_SkillTree::GetSkill(FGameplayTag SkillTag, FWarriorSkillDefinition& OutDefinition) const
{
	if (const FWarriorSkillDefinition* Found = FindSkill(SkillTag))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

#if WITH_EDITOR
EDataValidationResult UDataAsset_SkillTree::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	auto AddError = [&](const FText& InMessage)
	{
		Context.AddError(InMessage);
		Result = EDataValidationResult::Invalid;
	};

	TSet<FGameplayTag> SeenTags;
	for (int32 Index = 0; Index < Skills.Num(); ++Index)
	{
		const FWarriorSkillDefinition& Skill = Skills[Index];
		if (!Skill.SkillTag.IsValid())
		{
			AddError(FText::Format(LOCTEXT("Invalid_EmptyTag", "Skills[{0}] has no SkillTag."), Index));
			continue;
		}
		if (SeenTags.Contains(Skill.SkillTag))
		{
			AddError(FText::Format(LOCTEXT("Invalid_Duplicate", "Skill {0} is listed more than once."), FText::FromString(Skill.SkillTag.ToString())));
		}
		SeenTags.Add(Skill.SkillTag);

		for (const FGameplayTag& Prerequisite : Skill.Prerequisites)
		{
			if (Prerequisite == Skill.SkillTag)
			{
				AddError(FText::Format(LOCTEXT("Invalid_SelfPrereq", "Skill {0} lists itself as a prerequisite."), FText::FromString(Skill.SkillTag.ToString())));
			}
			else if (!FindSkill(Prerequisite))
			{
				AddError(FText::Format(LOCTEXT("Invalid_MissingPrereq", "Skill {0} needs {1}, which is not in this tree."),
					FText::FromString(Skill.SkillTag.ToString()), FText::FromString(Prerequisite.ToString())));
			}
		}

		for (const FWarriorSkillRequirement& Requirement : Skill.Requirements)
		{
			if (Requirement.Type == EWarriorSkillRequirementType::ExtraStat && !Requirement.StatTag.IsValid())
			{
				AddError(FText::Format(LOCTEXT("Invalid_ExtraStat", "Skill {0} has an ExtraStat requirement without StatTag."), FText::FromString(Skill.SkillTag.ToString())));
			}
		}
	}

	// 선행 스킬 순환 검사 (A → B → A). 순환이 있으면 어느 쪽도 해금할 수 없다.
	for (const FWarriorSkillDefinition& Skill : Skills)
	{
		TSet<FGameplayTag> Visited;
		TArray<FGameplayTag> Stack = Skill.Prerequisites;
		while (Stack.Num() > 0)
		{
			const FGameplayTag Current = Stack.Pop(EAllowShrinking::No);
			if (Current == Skill.SkillTag)
			{
				AddError(FText::Format(LOCTEXT("Invalid_Cycle", "Skill {0} has a circular prerequisite."), FText::FromString(Skill.SkillTag.ToString())));
				break;
			}
			if (Visited.Contains(Current))
			{
				continue;
			}
			Visited.Add(Current);
			if (const FWarriorSkillDefinition* Next = FindSkill(Current))
			{
				Stack.Append(Next->Prerequisites);
			}
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
