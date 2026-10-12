#include "WarriorSkillLibrary.h"

#include "DataAsset_SkillTree.h"
#include "ProjectWarrior/Account/WarriorAccountSubsystem.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Network/WarriorSkillUnlockSubsystem.h"
#include "ProjectWarrior/Network/WarriorSkillUnlockTypes.h"
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

void UWarriorSkillLibrary::ApplyServerProgress(TArray<FWarriorSkillView>& Views, const TArray<FWarriorServerSkillView>& ServerSkills)
{
	for (FWarriorSkillView& View : Views)
	{
		const FWarriorServerSkillView* Server = ServerSkills.FindByPredicate([&View](const FWarriorServerSkillView& Candidate)
		{
			return Candidate.SkillTag == View.Definition.SkillTag;
		});
		if (!Server)
		{
			// DA에만 있는 스킬은 서버가 해금하지 않는다(skills.json에 없음). 누르지 못하게 잠금으로 둔다.
			if (!View.IsUnlocked())
			{
				View.State = EWarriorSkillState::Locked;
			}
			continue;
		}
		// 진행도는 해금한 스킬도 서버 값으로 그린다(로컬 통계는 이 PC 것이라 서버 기록보다 적을 수 있다. 예: 클리어 1/1이 0/1로 보임).
		ApplyServerRequirements(View, Server->Requirements);
		// 상태는 계정 값(해금 응답·로그인 응답의 unlockedSkills)에 이미 해금으로 있으면 그대로 둔다.
		// 해금 직후에는 계정 값이 먼저 바뀌고 서버 진행도는 다시 받는 중이라, 지난 진행도가 해금 전 상태일 수 있다.
		if (!View.IsUnlocked())
		{
			View.State = Server->State;
		}
	}
}

void UWarriorSkillLibrary::ApplyServerRequirements(FWarriorSkillView& View, const TArray<FWarriorServerSkillRequirement>& Requirements)
{
	// 조건이 없으면(예전 서버는 해금한 스킬의 조건을 보내지 않았음) 로컬 값 그대로 둔다.
	if (Requirements.IsEmpty())
	{
		return;
	}

	View.MissingPrerequisites.Reset();
	for (const FWarriorServerSkillRequirement& Requirement : Requirements)
	{
		if (Requirement.Type == TEXT("PREREQUISITE") && !Requirement.bMet)
		{
			const FGameplayTag Prerequisite = FGameplayTag::RequestGameplayTag(FName(Requirement.Key), false);
			if (Prerequisite.IsValid())
			{
				View.MissingPrerequisites.Add(Prerequisite);
			}
		}
	}

	// DA 조건 순서대로, 같은 종류의 서버 조건을 하나씩 짝짓는다(같은 종류가 둘이면 앞에서부터).
	TArray<bool> Used;
	Used.Init(false, Requirements.Num());
	const int32 Count = FMath::Min(View.Definition.Requirements.Num(), View.Requirements.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const EWarriorSkillRequirementType LocalType = View.Definition.Requirements[Index].Type;
		for (int32 ServerIndex = 0; ServerIndex < Requirements.Num(); ++ServerIndex)
		{
			EWarriorSkillRequirementType ServerType;
			if (Used[ServerIndex] || !UWarriorSkillUnlockSubsystem::ToRequirementType(Requirements[ServerIndex].Type, ServerType)
				|| ServerType != LocalType)
			{
				continue;
			}
			const FWarriorServerSkillRequirement& Server = Requirements[ServerIndex];
			FWarriorSkillRequirementProgress& Progress = View.Requirements[Index];
			Progress.Target = Server.Target;
			Progress.Current = Server.bHasCurrent ? Server.Current : 0.0;
			Progress.bHasValue = Server.bHasCurrent;
			Progress.bMet = Server.bMet;
			Progress.bUpperBound = Server.bUpperBound;
			Used[ServerIndex] = true;
			break;
		}
	}
}
