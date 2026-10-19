#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"
#include "ProjectWarrior/Stage/WarriorMonsterCatalogDataAsset.h"
#include "ProjectWarrior/Stage/WarriorAffixCatalogDataAsset.h"
#include "ProjectWarrior/Stage/WarriorWavePlan.h"

namespace WarriorWavePlanTests
{
	//판 응답(stage-play-api.md v1.3 waves[]) 모양. spawnGroup null, 덮어쓴 골드, 보스 웨이브를 섞었다
	const TCHAR* ResponseBody = TEXT(R"({
		"data": {
			"stagePlayId": "3f2b8c1e-7a4d-4e2f-9b10-6c5d4e3f2a1b",
			"stageId": "stage.01.01",
			"waveCount": 2,
			"waveVersion": "w1-m1",
			"waves": [
				{ "waveNumber": 1, "restSeconds": 0, "bossWave": false, "goldChanceMultiplier": 1.0, "goldAmountMultiplier": 1.5,
				  "enemies": [ { "monsterId": "monster.samurai", "count": 2, "boss": false, "spawnGroup": null, "goldReward": 3, "goldDropChance": 1.0 } ] },
				{ "waveNumber": 2, "restSeconds": 12.5, "bossWave": true, "goldChanceMultiplier": 0.5, "goldAmountMultiplier": 1.0,
				  "enemies": [ { "monsterId": "monster.boss.warrior", "count": 1, "boss": true, "spawnGroup": "arena", "goldReward": 0, "goldDropChance": 1.0 },
				               { "monsterId": "monster.samurai", "count": 3, "boss": false, "spawnGroup": null, "goldReward": 0, "goldDropChance": 0.5 } ] }
			],
			"status": "IN_PROGRESS", "endReason": null, "endedAt": null
		},
		"meta": { "requestId": "0b6f3c1e-8a2d-4c6e-9f10-2a7b5d9e1c34" }
	})");

	UWarriorMonsterCatalogDataAsset* MakeCatalog(bool bWithBoss)
	{
		UWarriorMonsterCatalogDataAsset* Catalog = NewObject<UWarriorMonsterCatalogDataAsset>();
		TMap<FName, TSoftClassPtr<AWarriorAICharacter>> Monsters;
		Monsters.Add(TEXT("monster.samurai"), TSoftClassPtr<AWarriorAICharacter>(AWarriorAICharacter::StaticClass()));
		if (bWithBoss)
		{
			Monsters.Add(TEXT("monster.boss.warrior"), TSoftClassPtr<AWarriorAICharacter>(AWarriorAICharacter::StaticClass()));
		}
		Catalog->SetMonstersForTest(Monsters);
		return Catalog;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorWavePlanReadTest, "ProjectWarrior.Stage.M24.WavePlanRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorWavePlanReadTest::RunTest(const FString& Parameters)
{
	// 판 응답의 waveVersion·waves를 손으로 읽는다. null인 spawnGroup은 None, 칸 값은 그대로.
	// 실패하면 WarriorWavePlan::ReadFromData의 칸 이름(명세 waves[])을 의심한다
	FWarriorStageWavePlan Plan;
	WarriorWavePlan::ReadFromResponse(WarriorWavePlanTests::ResponseBody, Plan);

	TestEqual(TEXT("Version"), Plan.WaveVersion, FString(TEXT("w1-m1")));
	if (!TestEqual(TEXT("Wave count"), Plan.Waves.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Wave 2 number"), Plan.Waves[1].WaveNumber, 2);
	TestEqual(TEXT("Wave 2 rest"), Plan.Waves[1].RestSeconds, 12.5f);
	TestTrue(TEXT("Wave 2 boss wave"), Plan.Waves[1].bBossWave);
	TestEqual(TEXT("Wave 1 gold amount multiplier"), Plan.Waves[0].GoldAmountMultiplier, 1.5f);
	TestEqual(TEXT("Wave 2 enemies"), Plan.Waves[1].Enemies.Num(), 2);
	TestTrue(TEXT("Boss monster id"), Plan.Waves[1].Enemies[0].MonsterId == FName(TEXT("monster.boss.warrior")));
	TestTrue(TEXT("Boss flag"), Plan.Waves[1].Enemies[0].bBoss);
	TestTrue(TEXT("Spawn group"), Plan.Waves[1].Enemies[0].SpawnGroup == FName(TEXT("arena")));
	TestTrue(TEXT("Null spawn group is None"), Plan.Waves[0].Enemies[0].SpawnGroup.IsNone());
	TestEqual(TEXT("Gold reward override"), Plan.Waves[0].Enemies[0].GoldReward, 3);
	TestEqual(TEXT("Gold drop chance"), Plan.Waves[1].Enemies[1].GoldDropChance, 0.5f);

	// waves가 null(v1.3 전 플레이를 서버가 채우지 못함)·없음(옛 서버)·본문이 깨짐 → 빈 구성(IsSet false)
	FWarriorStageWavePlan NullPlan;
	WarriorWavePlan::ReadFromResponse(TEXT(R"({"data":{"stagePlayId":"a","waveVersion":null,"waves":null},"meta":{}})"), NullPlan);
	TestFalse(TEXT("Null waves"), NullPlan.IsSet());
	TestTrue(TEXT("Null version"), NullPlan.WaveVersion.IsEmpty());

	FWarriorStageWavePlan MissingPlan;
	WarriorWavePlan::ReadFromResponse(TEXT(R"({"data":{"stagePlayId":"a"},"meta":{}})"), MissingPlan);
	TestFalse(TEXT("Missing waves"), MissingPlan.IsSet());

	FWarriorStageWavePlan BrokenPlan;
	WarriorWavePlan::ReadFromResponse(TEXT("not json"), BrokenPlan);
	TestFalse(TEXT("Broken body"), BrokenPlan.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorWavePlanConvertTest, "ProjectWarrior.Stage.M24.WavePlanConvert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorWavePlanConvertTest::RunTest(const FString& Parameters)
{
	// 서버 구성 → FlowManager 웨이브 데이터. 몬스터 키는 카탈로그로 BP를 찾는다.
	// 하나라도 모르는 키가 있으면 판 전체를 만들지 않는다(D12, 일부만 하지 않기).
	// 실패하면 WarriorWavePlan::ToWaveData·FindUnknownMonster를 의심한다
	FWarriorStageWavePlan Plan;
	WarriorWavePlan::ReadFromResponse(WarriorWavePlanTests::ResponseBody, Plan);

	TArray<FWarriorStageWaveData> Waves;
	FName Unknown;
	TestTrue(TEXT("Converted"), WarriorWavePlan::ToWaveData(Plan, WarriorWavePlanTests::MakeCatalog(true), Waves, Unknown));
	if (!TestEqual(TEXT("Wave data count"), Waves.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Rest override"), Waves[1].RestTimeOverride, 12.5f);
	TestTrue(TEXT("Boss wave"), Waves[1].bBossWave);
	TestEqual(TEXT("Chance multiplier"), Waves[1].DropModifier.GoldDropChanceMultiplier, 0.5f);
	TestEqual(TEXT("Amount multiplier"), Waves[0].DropModifier.GoldAmountMultiplier, 1.5f);
	TestTrue(TEXT("Enemy class"), Waves[0].Enemies[0].EnemyClass.Get() == AWarriorAICharacter::StaticClass());
	TestEqual(TEXT("Count"), Waves[1].Enemies[1].Count, 3);
	TestTrue(TEXT("Boss enemy"), Waves[1].Enemies[0].bBossEnemy);
	TestTrue(TEXT("Spawn group data"), Waves[1].Enemies[0].SpawnGroup == FName(TEXT("arena")));
	TestEqual(TEXT("Gold reward"), Waves[0].Enemies[0].GoldReward, 3);
	TestEqual(TEXT("Gold chance"), Waves[1].Enemies[1].GoldDropChance, 0.5f);

	// 카탈로그에 보스가 없음 → 실패, 모르는 키를 알려 줌, 만든 웨이브는 비움
	TestFalse(TEXT("Unknown monster fails"), WarriorWavePlan::ToWaveData(Plan, WarriorWavePlanTests::MakeCatalog(false), Waves, Unknown));
	TestTrue(TEXT("Unknown key"), Unknown == FName(TEXT("monster.boss.warrior")));
	TestEqual(TEXT("No partial waves"), Waves.Num(), 0);

	// 이어하기 팝업 판단(CanResumeInThisBuild)이 쓰는 검사도 같은 결과
	TestTrue(TEXT("FindUnknownMonster"), WarriorWavePlan::FindUnknownMonster(Plan, WarriorWavePlanTests::MakeCatalog(false), Unknown));
	TestFalse(TEXT("All known"), WarriorWavePlan::FindUnknownMonster(Plan, WarriorWavePlanTests::MakeCatalog(true), Unknown));

	// 카탈로그를 지정하지 않음 → 모르는 몬스터와 같게 실패
	TestFalse(TEXT("No catalog fails"), WarriorWavePlan::ToWaveData(Plan, nullptr, Waves, Unknown));

	// 빈 구성 → 실패(모르는 키 없음)
	TestFalse(TEXT("Empty plan fails"), WarriorWavePlan::ToWaveData(FWarriorStageWavePlan(), WarriorWavePlanTests::MakeCatalog(true), Waves, Unknown));
	TestTrue(TEXT("Empty plan has no unknown key"), Unknown.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorMonsterIdTest, "ProjectWarrior.Stage.M24.MonsterId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorMonsterIdTest::RunTest(const FString& Parameters)
{
	// 몬스터 키 모양: 서버 기동 검사(StageWaveMasterValidator)와 같은 규칙. 실패하면 IsValidMonsterId를 의심한다
	TestTrue(TEXT("Simple"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.samurai")));
	TestTrue(TEXT("Dots and digits"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.boss.warrior2")));
	TestFalse(TEXT("No prefix"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("samurai")));
	TestFalse(TEXT("Prefix only"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.")));
	TestFalse(TEXT("Uppercase"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.Samurai")));
	TestFalse(TEXT("Underscore"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.samurai_elite")));
	TestFalse(TEXT("Too long"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.") + FString::ChrN(57, TEXT('a'))));
	TestTrue(TEXT("Max length"), UWarriorMonsterCatalogDataAsset::IsValidMonsterId(TEXT("monster.") + FString::ChrN(56, TEXT('a'))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStageStartErrorTest, "ProjectWarrior.Stage.M24.StageStartError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorStageStartErrorTest::RunTest(const FString& Parameters)
{
	// 판 시작 실패는 문구만 표에서 가져오고, 늘 팝업 + 메인메뉴다(표의 Retry·Inline이어도).
	// 새 게임 코드 세 줄이 표(STAGE_ 구역)에 있다. 실패하면 MakeStageStartError·WarriorErrorText.cpp를 의심한다
	FWarriorApiError Network;
	Network.Code = TEXT("NETWORK_ERROR");
	const FWarriorApiResult NetworkResult = UWarriorStagePlaySubsystem::MakeStageStartError(Network);
	TestFalse(TEXT("Not ok"), NetworkResult.bOk);
	TestTrue(TEXT("Network popup"), NetworkResult.Rule.Presentation == EWarriorErrorPresentation::Popup);
	TestTrue(TEXT("Network goes to main menu"), NetworkResult.Rule.Action == EWarriorErrorAction::GoMainMenu);
	TestTrue(TEXT("Network text from table"), NetworkResult.Rule.Text.EqualTo(FWarriorErrorText::GetText(TEXT("NETWORK_ERROR"))));

	FWarriorApiError Validation;
	Validation.Code = TEXT("VALIDATION_FAILED");
	TestTrue(TEXT("Inline code still popup"), UWarriorStagePlaySubsystem::MakeStageStartError(Validation).Rule.Presentation == EWarriorErrorPresentation::Popup);

	for (const TCHAR* Code : { TEXT("STAGE_WAVE_PLAN_TIMEOUT"), TEXT("STAGE_WAVE_PLAN_MISSING"), TEXT("STAGE_WAVE_MONSTER_UNKNOWN") })
	{
		const FWarriorErrorRule* Rule = FWarriorErrorText::Find(Code);
		if (TestNotNull(*FString::Printf(TEXT("%s in table"), Code), Rule))
		{
			TestTrue(*FString::Printf(TEXT("%s popup"), Code), Rule->Presentation == EWarriorErrorPresentation::Popup);
			TestTrue(*FString::Printf(TEXT("%s main menu"), Code), Rule->Action == EWarriorErrorAction::GoMainMenu);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorWavePlanDifficultyTest, "ProjectWarrior.Stage.M9.WavePlanDifficulty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorWavePlanDifficultyTest::RunTest(const FString& Parameters)
{
	// M9(stage-play-api.md v1.4): 판의 difficulty와 적 줄 healthMultiplier·damageMultiplier를 읽어 생성 데이터까지 넘긴다.
	// 칸이 없으면(M9 전 서버·구성) 난이도 0·배율 1, 0 이하가 와도 1(체력 0인 적이 생기지 않게).
	// 실패하면 WarriorWavePlan::ReadFromData의 칸 이름과 ToWaveData의 복사를 의심한다
	const TCHAR* Body = TEXT(R"({
		"data": {
			"stagePlayId": "3f2b8c1e-7a4d-4e2f-9b10-6c5d4e3f2a1b",
			"difficulty": 2,
			"waveVersion": "w2-m2-d1",
			"waves": [
				{ "waveNumber": 1, "restSeconds": 0, "bossWave": false, "goldChanceMultiplier": 1.0, "goldAmountMultiplier": 1.0,
				  "enemies": [ { "monsterId": "monster.samurai", "count": 5, "boss": false, "spawnGroup": null, "goldReward": 2, "goldDropChance": 1.0,
				                 "healthMultiplier": 1.5, "damageMultiplier": 1.3 },
				               { "monsterId": "monster.samurai", "count": 1, "boss": false, "spawnGroup": null, "goldReward": 2, "goldDropChance": 1.0,
				                 "healthMultiplier": 0, "damageMultiplier": -1 } ] },
				{ "waveNumber": 2, "restSeconds": 0, "bossWave": false, "goldChanceMultiplier": 1.0, "goldAmountMultiplier": 1.0,
				  "enemies": [ { "monsterId": "monster.samurai", "count": 2, "boss": false, "spawnGroup": null, "goldReward": 2, "goldDropChance": 1.0 } ] }
			]
		},
		"meta": {}
	})");

	FWarriorStageWavePlan Plan;
	WarriorWavePlan::ReadFromResponse(Body, Plan);
	TestEqual(TEXT("Difficulty"), Plan.Difficulty, 2);
	if (!TestEqual(TEXT("Wave count"), Plan.Waves.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Health multiplier"), Plan.Waves[0].Enemies[0].HealthMultiplier, 1.5f);
	TestEqual(TEXT("Damage multiplier"), Plan.Waves[0].Enemies[0].DamageMultiplier, 1.3f);
	TestEqual(TEXT("Zero health multiplier becomes 1"), Plan.Waves[0].Enemies[1].HealthMultiplier, 1.f);
	TestEqual(TEXT("Negative damage multiplier becomes 1"), Plan.Waves[0].Enemies[1].DamageMultiplier, 1.f);
	TestEqual(TEXT("Missing health multiplier is 1"), Plan.Waves[1].Enemies[0].HealthMultiplier, 1.f);
	TestEqual(TEXT("Missing damage multiplier is 1"), Plan.Waves[1].Enemies[0].DamageMultiplier, 1.f);

	TArray<FWarriorStageWaveData> Waves;
	FName Unknown;
	TestTrue(TEXT("Converted"), WarriorWavePlan::ToWaveData(Plan, WarriorWavePlanTests::MakeCatalog(false), Waves, Unknown));
	if (TestEqual(TEXT("Wave data count"), Waves.Num(), 2))
	{
		TestEqual(TEXT("Spawn data health"), Waves[0].Enemies[0].HealthMultiplier, 1.5f);
		TestEqual(TEXT("Spawn data damage"), Waves[0].Enemies[0].DamageMultiplier, 1.3f);
		TestEqual(TEXT("Spawn data default health"), Waves[1].Enemies[0].HealthMultiplier, 1.f);
	}

	// M9 전 응답(difficulty 칸 없음)은 난이도 0
	FWarriorStageWavePlan OldPlan;
	WarriorWavePlan::ReadFromResponse(WarriorWavePlanTests::ResponseBody, OldPlan);
	TestEqual(TEXT("Old response difficulty"), OldPlan.Difficulty, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorWavePlanAffixTest, "ProjectWarrior.Stage.Affix.WavePlanAffixes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorWavePlanAffixTest::RunTest(const FString& Parameters)
{
	// 수식어(stage-play-api.md v1.5): 적 줄 affixes를 순서대로 읽어 생성 데이터까지 넘긴다.
	// 칸이 없거나(v1.5 전) null·빈 배열이면 비움. 문자열이 아닌 값·빈 문자열·겹친 키는 건너뛴다(모르는 키는 적에게 붙일 때 거름).
	// 실패하면 WarriorWavePlan::ReadFromData의 affixes 읽기와 ToWaveData의 복사를 의심한다
	const TCHAR* Body = TEXT(R"({
		"data": {
			"stagePlayId": "3f2b8c1e-7a4d-4e2f-9b10-6c5d4e3f2a1b",
			"waves": [
				{ "waveNumber": 1, "enemies": [
					{ "monsterId": "monster.samurai", "count": 2, "affixes": ["affix.fire", "affix.fast"] },
					{ "monsterId": "monster.samurai", "count": 1, "affixes": [] },
					{ "monsterId": "monster.samurai", "count": 1, "affixes": null },
					{ "monsterId": "monster.samurai", "count": 1 },
					{ "monsterId": "monster.samurai", "count": 1, "affixes": ["affix.frozen", 3, "", "affix.frozen", "affix.unknown"] }
				] }
			]
		},
		"meta": {}
	})");

	FWarriorStageWavePlan Plan;
	WarriorWavePlan::ReadFromResponse(Body, Plan);
	if (!TestEqual(TEXT("Wave count"), Plan.Waves.Num(), 1) || !TestEqual(TEXT("Enemy lines"), Plan.Waves[0].Enemies.Num(), 5))
	{
		return false;
	}
	const TArray<FWarriorWavePlanEnemy>& Enemies = Plan.Waves[0].Enemies;
	TestEqual(TEXT("Two affixes"), Enemies[0].Affixes.Num(), 2);
	TestTrue(TEXT("Order kept"), Enemies[0].Affixes.Num() == 2 && Enemies[0].Affixes[0] == FName(TEXT("affix.fire")) && Enemies[0].Affixes[1] == FName(TEXT("affix.fast")));
	TestTrue(TEXT("Empty array"), Enemies[1].Affixes.IsEmpty());
	TestTrue(TEXT("Null affixes"), Enemies[2].Affixes.IsEmpty());
	TestTrue(TEXT("Missing affixes (before v1.5)"), Enemies[3].Affixes.IsEmpty());
	TestEqual(TEXT("Non-string, empty and duplicate skipped; unknown kept for later"), Enemies[4].Affixes.Num(), 2);

	TArray<FWarriorStageWaveData> Waves;
	FName Unknown;
	TestTrue(TEXT("Converted"), WarriorWavePlan::ToWaveData(Plan, WarriorWavePlanTests::MakeCatalog(false), Waves, Unknown));
	if (TestEqual(TEXT("Wave data count"), Waves.Num(), 1))
	{
		TestEqual(TEXT("Spawn data affixes"), Waves[0].Enemies[0].Affixes.Num(), 2);
		TestTrue(TEXT("Spawn data no affixes"), Waves[0].Enemies[3].Affixes.IsEmpty());
	}

	// 수식어가 없는 예전 응답도 그대로 읽힌다
	FWarriorStageWavePlan OldPlan;
	WarriorWavePlan::ReadFromResponse(WarriorWavePlanTests::ResponseBody, OldPlan);
	TestTrue(TEXT("Old response has no affixes"), OldPlan.Waves.Num() > 0 && OldPlan.Waves[0].Enemies[0].Affixes.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorAffixCatalogTest, "ProjectWarrior.Stage.Affix.CatalogRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorAffixCatalogTest::RunTest(const FString& Parameters)
{
	// 수식어 키 모양은 서버 검사(StageWaveMasterValidator W8)와 같은 규칙, 등급은 붙은 수 1 = 챔피언, 2+ = 희귀.
	// 실패하면 UWarriorAffixCatalogDataAsset::IsValidAffixId·GetTierForCount·FindAffix를 의심한다
	TestTrue(TEXT("Simple"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.fire")));
	TestTrue(TEXT("Dots and digits"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.vampiric.lv2")));
	TestFalse(TEXT("No prefix"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("fire")));
	TestFalse(TEXT("Prefix only"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.")));
	TestFalse(TEXT("Uppercase"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.Fire")));
	TestFalse(TEXT("Underscore"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.fire_aura")));
	TestTrue(TEXT("Max length"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.") + FString::ChrN(58, TEXT('a'))));
	TestFalse(TEXT("Too long"), UWarriorAffixCatalogDataAsset::IsValidAffixId(TEXT("affix.") + FString::ChrN(59, TEXT('a'))));

	TestTrue(TEXT("Tier none"), UWarriorAffixCatalogDataAsset::GetTierForCount(0) == EWarriorAffixTier::None);
	TestTrue(TEXT("Tier champion"), UWarriorAffixCatalogDataAsset::GetTierForCount(1) == EWarriorAffixTier::Champion);
	TestTrue(TEXT("Tier rare"), UWarriorAffixCatalogDataAsset::GetTierForCount(2) == EWarriorAffixTier::Rare);
	TestTrue(TEXT("Tier rare (4)"), UWarriorAffixCatalogDataAsset::GetTierForCount(4) == EWarriorAffixTier::Rare);

	UWarriorAffixCatalogDataAsset* Catalog = NewObject<UWarriorAffixCatalogDataAsset>();
	TMap<FName, FWarriorAffixDefinition> Affixes;
	Affixes.Add(TEXT("affix.fire"), FWarriorAffixDefinition());
	Catalog->SetAffixesForTest(Affixes);
	TestNotNull(TEXT("Known affix"), Catalog->FindAffix(TEXT("affix.fire")));
	TestNull(TEXT("Unknown affix"), Catalog->FindAffix(TEXT("affix.unknown")));
	TestNull(TEXT("None key"), Catalog->FindAffix(NAME_None));
	return true;
}

#endif
