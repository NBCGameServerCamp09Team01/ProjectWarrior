#include "WarriorSkillLibrary.h"

#include "DataAsset_SkillTree.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Stats/WarriorProfileStatsSubsystem.h"

bool UWarriorSkillLibrary::MakeEvalContext(const UObject* WorldContextObject, FWarriorSkillEvalContext& OutContext)
{
	OutContext = FWarriorSkillEvalContext();

	const UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(WorldContextObject);
	if (!Account)
	{
		return false;
	}

	const FWarriorAccountData& AccountData = Account->GetAccountData();
	OutContext.AccountLevel = AccountData.AccountLevel;
	OutContext.StatPoints = AccountData.StatPoints;
	OutContext.UnlockedSkills = AccountData.UnlockedSkills;

	if (const UWarriorProfileStatsSubsystem* ProfileStats = UWarriorProfileStatsSubsystem::Get(WorldContextObject))
	{
		OutContext.Lifetime = ProfileStats->GetLifetimeStats();
	}
	return true;
}

FWarriorSkillRequirementProgress UWarriorSkillLibrary::EvaluateRequirement(const FWarriorSkillRequirement& Requirement, const FWarriorSkillEvalContext& Context)
{
	FWarriorSkillRequirementProgress Progress;
	Progress.Text = Requirement.GetDisplayText();
	Progress.Target = Requirement.TargetValue;
	Progress.bUpperBound = Requirement.IsUpperBound();

	const FWarriorLifetimeStats& Lifetime = Context.Lifetime;
	const FWarriorAttackStats& Attack = Lifetime.Stats.Attack;

	switch (Requirement.Type)
	{
	case EWarriorSkillRequirementType::AccountLevel:
		Progress.Current = Context.AccountLevel;
		break;
	case EWarriorSkillRequirementType::TotalKills:
		// 플레이어가 막타를 친 처치만 센다 (S1 Attack.Kills 기준).
		Progress.Current = Requirement.Key.IsNone() ? Attack.Kills : Attack.KillsByEnemyType.FindRef(Requirement.Key);
		break;
	case EWarriorSkillRequirementType::KillsByDeathType:
		Progress.Current = Attack.KillsByDeathType.FindRef(Requirement.Key);
		break;
	case EWarriorSkillRequirementType::StagesCleared:
		Progress.Current = Lifetime.StagesCleared;
		break;
	case EWarriorSkillRequirementType::PlayTimeSeconds:
		Progress.Current = Lifetime.Stats.PlayTimeSeconds;
		break;
	case EWarriorSkillRequirementType::MaxWaveReached:
		Progress.Current = Lifetime.MaxWaveReached;
		break;
	case EWarriorSkillRequirementType::BestClearTime:
		// 클리어 기록이 없으면 0으로 남아 있다. 0초 클리어로 착각하지 않게 기록 없음으로 둔다.
		Progress.bHasValue = Lifetime.BestStageClearTimeSeconds > 0.0f;
		Progress.Current = Lifetime.BestStageClearTimeSeconds;
		break;
	case EWarriorSkillRequirementType::TotalDamage:
		Progress.Current = Attack.DamageDealt;
		break;
	case EWarriorSkillRequirementType::MaxDamage:
		Progress.Current = Attack.MaxDamageDealt;
		break;
	case EWarriorSkillRequirementType::ExtraStat:
		if (const FWarriorStatValue* Value = Lifetime.Stats.FindExtra(Requirement.StatTag, Requirement.Key))
		{
			Progress.Current = Value->Sum;
		}
		break;
	}

	Progress.bMet = Progress.bUpperBound
		? (Progress.bHasValue && Progress.Current <= Progress.Target)
		: (Progress.Current >= Progress.Target);
	return Progress;
}

FWarriorSkillView UWarriorSkillLibrary::EvaluateSkill(const FWarriorSkillDefinition& Definition, const FWarriorSkillEvalContext& Context)
{
	FWarriorSkillView View;
	View.Definition = Definition;

	bool bAllMet = true;
	for (const FWarriorSkillRequirement& Requirement : Definition.Requirements)
	{
		const FWarriorSkillRequirementProgress& Progress = View.Requirements.Add_GetRef(EvaluateRequirement(Requirement, Context));
		bAllMet &= Progress.bMet;
	}

	for (const FGameplayTag& Prerequisite : Definition.Prerequisites)
	{
		if (!Context.UnlockedSkills.HasTagExact(Prerequisite))
		{
			View.MissingPrerequisites.Add(Prerequisite);
		}
	}

	// 한 번 해금한 스킬은 조건이 다시 내려가도 해금 상태를 유지한다. 진행도는 그대로 보여 준다.
	if (Context.UnlockedSkills.HasTagExact(Definition.SkillTag))
	{
		View.State = EWarriorSkillState::Unlocked;
	}
	else if (!bAllMet || View.MissingPrerequisites.Num() > 0)
	{
		View.State = EWarriorSkillState::Locked;
	}
	else if (Context.StatPoints < Definition.StatPointCost)
	{
		View.State = EWarriorSkillState::NeedPoints;
	}
	else
	{
		View.State = EWarriorSkillState::Unlockable;
	}
	return View;
}

TArray<FWarriorSkillView> UWarriorSkillLibrary::EvaluateSkillTree(const UDataAsset_SkillTree* SkillTree, const FWarriorSkillEvalContext& Context)
{
	TArray<FWarriorSkillView> Views;
	if (!SkillTree)
	{
		return Views;
	}

	Views.Reserve(SkillTree->Skills.Num());
	for (const FWarriorSkillDefinition& Definition : SkillTree->Skills)
	{
		Views.Add(EvaluateSkill(Definition, Context));
	}
	return Views;
}

TArray<FWarriorSkillView> UWarriorSkillLibrary::BuildSkillViews(const UObject* WorldContextObject, const UDataAsset_SkillTree* SkillTree)
{
	FWarriorSkillEvalContext Context;
	MakeEvalContext(WorldContextObject, Context);
	return EvaluateSkillTree(SkillTree, Context);
}

bool UWarriorSkillLibrary::TryUnlockSkill(const UObject* WorldContextObject, const UDataAsset_SkillTree* SkillTree, FGameplayTag SkillTag)
{
	const FWarriorSkillDefinition* Definition = SkillTree ? SkillTree->FindSkill(SkillTag) : nullptr;
	if (!Definition)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Skill] Unlock ignored. %s is not in the skill tree."), *SkillTag.ToString());
		return false;
	}

	UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(WorldContextObject);
	FWarriorSkillEvalContext Context;
	if (!Account || !MakeEvalContext(WorldContextObject, Context))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Skill] Unlock ignored. No account subsystem."));
		return false;
	}

	// 화면이 열려 있는 동안 값이 바뀌었을 수 있으니 해금 직전에 다시 평가한다.
	const FWarriorSkillView View = EvaluateSkill(*Definition, Context);
	if (!View.CanUnlock())
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Skill] Unlock rejected. %s is %s."), *SkillTag.ToString(), *UEnum::GetValueAsString(View.State));
		return false;
	}

	// 비용은 스킬 정의(DA_SkillTree)가 기준이다. 포인트 차감과 해금 기록은 계정이 한다.
	const bool bUnlocked = Account->UnlockSkillWithCost(SkillTag, Definition->StatPointCost);
	if (bUnlocked)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Skill] Unlocked %s."), *SkillTag.ToString());
	}
	return bUnlocked;
}
