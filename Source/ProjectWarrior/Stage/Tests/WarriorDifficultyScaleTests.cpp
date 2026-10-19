#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Stage/WarriorDifficultyScaleGate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorDifficultyScaleOrderTest, "ProjectWarrior.Stage.M9.DifficultyScaleOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorDifficultyScaleOrderTest::RunTest(const FString& Parameters)
{
	// 배율과 시작 데이터가 어느 순서로 와도 둘 다 준비됐을 때 정확히 한 번 곱하는지 본다.
	// 실패하면 FWarriorDifficultyScaleGate::TryApply의 표시 순서(1이면 표시를 세우지 않음)를 의심한다

	// ① 배율 먼저 → 시작 데이터 나중 (웨이브 생성기: SpawnActor 직후 배율, 다음 프레임 이후 로드 콜백)
	{
		FWarriorDifficultyScaleGate Gate;
		TestFalse(TEXT("Scale first: wait for start-up data"), Gate.SetScale(1.5f, 1.3f));
		TestTrue(TEXT("Scale first: apply when start-up data arrives"), Gate.MarkStartUpDataApplied());
		TestFalse(TEXT("Scale first: not twice (start-up again)"), Gate.MarkStartUpDataApplied());
		TestFalse(TEXT("Scale first: not twice (scale again)"), Gate.SetScale(1.5f, 1.3f));
	}

	// ② 시작 데이터 먼저 → 배율 나중 (시작 데이터가 없는 적: 빙의 때 바로 "적용 끝"이 되고, SpawnActor가 돌아온 뒤 배율이 옴)
	{
		FWarriorDifficultyScaleGate Gate;
		TestFalse(TEXT("Start-up first: nothing to apply at 1.0"), Gate.MarkStartUpDataApplied());
		TestFalse(TEXT("Start-up first: 1.0 does not mark as applied"), Gate.bApplied);
		TestTrue(TEXT("Start-up first: apply when scale arrives"), Gate.SetScale(1.5f, 1.3f));
		TestFalse(TEXT("Start-up first: not twice"), Gate.SetScale(1.5f, 1.3f));
	}

	// ③ 이지(배율 1)는 어느 순서로도 곱하지 않는다
	{
		FWarriorDifficultyScaleGate ScaleFirst;
		TestFalse(TEXT("Easy, scale first: scale"), ScaleFirst.SetScale(1.f, 1.f));
		TestFalse(TEXT("Easy, scale first: start-up"), ScaleFirst.MarkStartUpDataApplied());

		FWarriorDifficultyScaleGate StartUpFirst;
		TestFalse(TEXT("Easy, start-up first: start-up"), StartUpFirst.MarkStartUpDataApplied());
		TestFalse(TEXT("Easy, start-up first: scale"), StartUpFirst.SetScale(1.f, 1.f));
	}

	// ④ 잘못 온 값(0 이하)은 1로 둔다 → 곱하지 않는다(체력 0인 적 방지)
	{
		FWarriorDifficultyScaleGate Gate;
		Gate.MarkStartUpDataApplied();
		TestFalse(TEXT("Invalid values: not applied"), Gate.SetScale(0.f, -1.f));
		TestEqual(TEXT("Invalid health becomes 1"), Gate.HealthMultiplier, 1.f);
		TestEqual(TEXT("Invalid damage becomes 1"), Gate.DamageMultiplier, 1.f);
	}

	// ⑤ 한쪽만 1이 아니어도 곱한다(체력 그대로, 공격력만 올리는 스테이지 덮어쓰기)
	{
		FWarriorDifficultyScaleGate Gate;
		Gate.MarkStartUpDataApplied();
		TestTrue(TEXT("Damage only: applied"), Gate.SetScale(1.f, 1.2f));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
