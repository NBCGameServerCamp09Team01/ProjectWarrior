#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Skill/DataAsset_SkillTree.h"
#include "ProjectWarrior/Skill/WarriorSkillLibrary.h"
#include "ProjectWarrior/Stats/WarriorStatTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

/**
 * SK 스킬 조건 평가 테스트.
 * 평가 함수는 입력(FWarriorSkillEvalContext)만 보므로 월드 없이 값을 직접 채워 검증한다.
 * 스킬 태그는 계정 태그 등록 전(SK-6)이라, 이미 등록된 통계 태그 두 개를 스킬 태그 대신 쓴다. 평가 로직은 태그 이름과 무관하다.
 */
namespace WarriorSkillTests_Private
{
	FWarriorSkillRequirement MakeRequirement(const EWarriorSkillRequirementType InType, const double InTarget, const FName InKey = NAME_None)
	{
		FWarriorSkillRequirement Requirement;
		Requirement.Type = InType;
		Requirement.TargetValue = InTarget;
		Requirement.Key = InKey;
		return Requirement;
	}

	FWarriorSkillDefinition MakeSkill(const FGameplayTag& InTag, const int32 InCost)
	{
		FWarriorSkillDefinition Skill;
		Skill.SkillTag = InTag;
		Skill.DisplayName = FText::FromString(InTag.ToString());
		Skill.StatPointCost = InCost;
		return Skill;
	}
}

namespace WSKT = WarriorSkillTests_Private;

#define WARRIOR_SKILL_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#define SKILL_A WarriorStatTags::Stat_Time_Rest.GetTag()
#define SKILL_B WarriorStatTags::Stat_Time_Shop.GetTag()

//~ 조건 진행도

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorSkillRequirementProgressTest, "ProjectWarrior.Skill.SK.RequirementProgress", WARRIOR_SKILL_TEST_FLAGS)

bool FWarriorSkillRequirementProgressTest::RunTest(const FString& Parameters)
{
	FWarriorSkillEvalContext Context;
	Context.AccountLevel = 3;
	Context.Lifetime.Stats.Attack.Kills = 42;
	Context.Lifetime.Stats.Attack.KillsByEnemyType.Add(TEXT("BP_SamuraiAI"), 30);
	Context.Lifetime.Stats.Attack.KillsByDeathType.Add(TEXT("Finisher"), 7);
	Context.Lifetime.StagesCleared = 2;
	Context.Lifetime.Stats.PlayTimeSeconds = 650.0;
	Context.Lifetime.MaxWaveReached = 5;
	Context.Lifetime.Stats.AddExtra(WarriorStatTags::Stat_Defense_Wave_NoHit, 1.0);
	Context.Lifetime.Stats.AddExtra(WarriorStatTags::Stat_Defense_Wave_NoHit, 1.0);

	using EType = EWarriorSkillRequirementType;
	auto Eval = [&](const FWarriorSkillRequirement& Requirement) { return UWarriorSkillLibrary::EvaluateRequirement(Requirement, Context); };

	FWarriorSkillRequirementProgress P = Eval(WSKT::MakeRequirement(EType::AccountLevel, 3));
	TestEqual(TEXT("Level current"), P.Current, 3.0);
	TestTrue(TEXT("Level 3 >= 3"), P.bMet);

	P = Eval(WSKT::MakeRequirement(EType::AccountLevel, 5));
	TestFalse(TEXT("Level 3 < 5"), P.bMet);
	TestEqual(TEXT("Level ratio"), P.GetRatio(), 0.6);

	P = Eval(WSKT::MakeRequirement(EType::TotalKills, 30));
	TestEqual(TEXT("Total kills"), P.Current, 42.0);
	TestTrue(TEXT("Total kills met"), P.bMet);
	TestEqual(TEXT("Ratio is clamped"), P.GetRatio(), 1.0);

	P = Eval(WSKT::MakeRequirement(EType::TotalKills, 50, TEXT("BP_SamuraiAI")));
	TestEqual(TEXT("Kills by enemy type"), P.Current, 30.0);
	TestFalse(TEXT("Kills by enemy type not met"), P.bMet);

	P = Eval(WSKT::MakeRequirement(EType::TotalKills, 1, TEXT("BP_ArcherAI")));
	TestEqual(TEXT("Unknown enemy type is 0"), P.Current, 0.0);

	P = Eval(WSKT::MakeRequirement(EType::KillsByDeathType, 10, TEXT("Finisher")));
	TestEqual(TEXT("Finisher kills"), P.Current, 7.0);
	TestFalse(TEXT("Finisher 7 < 10"), P.bMet);

	TestTrue(TEXT("Stages cleared"), Eval(WSKT::MakeRequirement(EType::StagesCleared, 2)).bMet);
	TestTrue(TEXT("Play time"), Eval(WSKT::MakeRequirement(EType::PlayTimeSeconds, 600)).bMet);
	TestTrue(TEXT("Max wave"), Eval(WSKT::MakeRequirement(EType::MaxWaveReached, 5)).bMet);
	TestFalse(TEXT("Damage is 0 until damage is recorded"), Eval(WSKT::MakeRequirement(EType::TotalDamage, 1)).bMet);

	FWarriorSkillRequirement NoHit = WSKT::MakeRequirement(EType::ExtraStat, 2);
	NoHit.StatTag = WarriorStatTags::Stat_Defense_Wave_NoHit;
	P = Eval(NoHit);
	TestEqual(TEXT("Extra stat sum"), P.Current, 2.0);
	TestTrue(TEXT("Extra stat met"), P.bMet);

	// 이하 조건: 기록이 없으면 미충족
	P = Eval(WSKT::MakeRequirement(EType::BestClearTime, 300));
	TestFalse(TEXT("No clear record"), P.bHasValue);
	TestFalse(TEXT("No clear record is not met"), P.bMet);

	Context.Lifetime.BestStageClearTimeSeconds = 280.0f;
	P = Eval(WSKT::MakeRequirement(EType::BestClearTime, 300));
	TestTrue(TEXT("280 <= 300"), P.bMet);
	TestEqual(TEXT("Upper bound ratio is 0 or 1"), P.GetRatio(), 1.0);

	Context.Lifetime.BestStageClearTimeSeconds = 312.0f;
	TestFalse(TEXT("312 > 300"), Eval(WSKT::MakeRequirement(EType::BestClearTime, 300)).bMet);

	// 기본 문구
	TestFalse(TEXT("Default text is generated"), WSKT::MakeRequirement(EType::TotalKills, 30).GetDisplayText().IsEmpty());
	FWarriorSkillRequirement Custom = WSKT::MakeRequirement(EType::TotalKills, 30);
	Custom.DisplayText = FText::FromString(TEXT("Custom"));
	TestEqual(TEXT("Custom text is kept"), Custom.GetDisplayText().ToString(), FString(TEXT("Custom")));
	return true;
}

//~ 스킬 상태

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorSkillStateTest, "ProjectWarrior.Skill.SK.State", WARRIOR_SKILL_TEST_FLAGS)

bool FWarriorSkillStateTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Test tags are valid"), SKILL_A.IsValid() && SKILL_B.IsValid()))
	{
		return false;
	}

	FWarriorSkillDefinition Skill = WSKT::MakeSkill(SKILL_B, 2);
	Skill.Requirements.Add(WSKT::MakeRequirement(EWarriorSkillRequirementType::AccountLevel, 3));
	Skill.Requirements.Add(WSKT::MakeRequirement(EWarriorSkillRequirementType::TotalKills, 30));
	Skill.Prerequisites.Add(SKILL_A);

	FWarriorSkillEvalContext Context;
	Context.AccountLevel = 3;
	Context.StatPoints = 5;
	Context.Lifetime.Stats.Attack.Kills = 10;

	// 조건 하나 미충족 → 잠김
	FWarriorSkillView View = UWarriorSkillLibrary::EvaluateSkill(Skill, Context);
	TestTrue(TEXT("Locked when a requirement is not met"), View.State == EWarriorSkillState::Locked);
	TestEqual(TEXT("Progress per requirement"), View.Requirements.Num(), 2);
	TestTrue(TEXT("Level requirement met"), View.Requirements[0].bMet);
	TestFalse(TEXT("Kill requirement not met"), View.Requirements[1].bMet);

	// 조건 충족, 선행 미해금 → 잠김
	Context.Lifetime.Stats.Attack.Kills = 42;
	View = UWarriorSkillLibrary::EvaluateSkill(Skill, Context);
	TestTrue(TEXT("Locked when a prerequisite is missing"), View.State == EWarriorSkillState::Locked);
	TestEqual(TEXT("Missing prerequisite listed"), View.MissingPrerequisites.Num(), 1);

	// 선행 해금, 포인트 부족 → NeedPoints
	Context.UnlockedSkills.AddTag(SKILL_A);
	Context.StatPoints = 1;
	View = UWarriorSkillLibrary::EvaluateSkill(Skill, Context);
	TestTrue(TEXT("NeedPoints when points are short"), View.State == EWarriorSkillState::NeedPoints);
	TestEqual(TEXT("No missing prerequisite"), View.MissingPrerequisites.Num(), 0);
	TestFalse(TEXT("Cannot unlock with NeedPoints"), View.CanUnlock());

	// 포인트 충분 → 해금 가능 (비용과 같으면 해금 가능)
	Context.StatPoints = 2;
	View = UWarriorSkillLibrary::EvaluateSkill(Skill, Context);
	TestTrue(TEXT("Unlockable"), View.State == EWarriorSkillState::Unlockable);
	TestTrue(TEXT("CanUnlock"), View.CanUnlock());

	// 해금 후에는 조건이 내려가도 해금됨 유지
	Context.UnlockedSkills.AddTag(SKILL_B);
	Context.Lifetime = FWarriorLifetimeStats();
	Context.StatPoints = 0;
	View = UWarriorSkillLibrary::EvaluateSkill(Skill, Context);
	TestTrue(TEXT("Unlocked stays unlocked"), View.State == EWarriorSkillState::Unlocked);
	TestFalse(TEXT("Progress still shown"), View.Requirements[1].bMet);
	TestFalse(TEXT("Cannot unlock again"), View.CanUnlock());
	return true;
}

//~ 조건 없는 스킬 (포인트만)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorSkillPointsOnlyTest, "ProjectWarrior.Skill.SK.PointsOnly", WARRIOR_SKILL_TEST_FLAGS)

bool FWarriorSkillPointsOnlyTest::RunTest(const FString& Parameters)
{
	const FWarriorSkillDefinition Skill = WSKT::MakeSkill(SKILL_A, 2);
	FWarriorSkillEvalContext Context;

	Context.StatPoints = 0;
	TestTrue(TEXT("No requirements, no points → NeedPoints"),
		UWarriorSkillLibrary::EvaluateSkill(Skill, Context).State == EWarriorSkillState::NeedPoints);

	Context.StatPoints = 2;
	TestTrue(TEXT("No requirements, enough points → Unlockable"),
		UWarriorSkillLibrary::EvaluateSkill(Skill, Context).State == EWarriorSkillState::Unlockable);

	const FWarriorSkillDefinition FreeSkill = WSKT::MakeSkill(SKILL_B, 0);
	Context.StatPoints = 0;
	TestTrue(TEXT("Zero cost → Unlockable"),
		UWarriorSkillLibrary::EvaluateSkill(FreeSkill, Context).State == EWarriorSkillState::Unlockable);
	return true;
}

//~ 스킬 목록

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorSkillTreeTest, "ProjectWarrior.Skill.SK.Tree", WARRIOR_SKILL_TEST_FLAGS)

bool FWarriorSkillTreeTest::RunTest(const FString& Parameters)
{
	UDataAsset_SkillTree* Tree = NewObject<UDataAsset_SkillTree>(GetTransientPackage());
	FWarriorSkillDefinition A = WSKT::MakeSkill(SKILL_A, 1);
	FWarriorSkillDefinition B = WSKT::MakeSkill(SKILL_B, 1);
	B.Prerequisites.Add(SKILL_A);
	Tree->Skills = { A, B };

	TestNotNull(TEXT("Find A"), Tree->FindSkill(SKILL_A));
	TestNull(TEXT("Find invalid tag"), Tree->FindSkill(FGameplayTag()));

	FWarriorSkillEvalContext Context;
	Context.StatPoints = 1;
	TArray<FWarriorSkillView> Views = UWarriorSkillLibrary::EvaluateSkillTree(Tree, Context);
	if (TestEqual(TEXT("Views in tree order"), Views.Num(), 2))
	{
		TestTrue(TEXT("A first"), Views[0].Definition.SkillTag == SKILL_A);
		TestTrue(TEXT("A unlockable"), Views[0].State == EWarriorSkillState::Unlockable);
		TestTrue(TEXT("B locked by prerequisite"), Views[1].State == EWarriorSkillState::Locked);
	}
	TestEqual(TEXT("Null tree gives no views"), UWarriorSkillLibrary::EvaluateSkillTree(nullptr, Context).Num(), 0);

#if WITH_EDITOR
	{
		FDataValidationContext ValidContext;
		TestTrue(TEXT("Valid tree"), Tree->IsDataValid(ValidContext) != EDataValidationResult::Invalid);
	}

	// 순환: A가 B를 선행으로 요구
	Tree->Skills[0].Prerequisites.Add(SKILL_B);
	{
		FDataValidationContext CycleContext;
		TestTrue(TEXT("Cycle is invalid"), Tree->IsDataValid(CycleContext) == EDataValidationResult::Invalid);
	}
	Tree->Skills[0].Prerequisites.Reset();

	// 중복 태그
	Tree->Skills.Add(A);
	{
		FDataValidationContext DuplicateContext;
		TestTrue(TEXT("Duplicate is invalid"), Tree->IsDataValid(DuplicateContext) == EDataValidationResult::Invalid);
	}
#endif
	return true;
}

#undef SKILL_A
#undef SKILL_B
#undef WARRIOR_SKILL_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
