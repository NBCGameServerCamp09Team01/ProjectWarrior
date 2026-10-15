#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"
#include "ProjectWarrior/Stage/WarriorWavePlan.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorStageLaunchResumeDifficultyTest, "ProjectWarrior.Stage.M9.ResumeDifficulty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorStageLaunchResumeDifficultyTest::RunTest(const FString& Parameters)
{
	// M9(통합 점검 I-줄4-01): 이어하기는 화면 선택(다시 켜면 0)이 아니라 그 판(P2 구성)의 난이도로 연다.
	// 실패하면 UWarriorStageLaunchSubsystem::SelectStageForResume이 InResume.WavePlan.Difficulty를 쓰는지 본다
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UWarriorStageLaunchSubsystem* Launch = NewObject<UWarriorStageLaunchSubsystem>(GameInstance);
	const FName StageId(TEXT("stage.01.02"));

	// 노말로 시작한 판을, 게임을 다시 켜서(화면 선택 0) 이어 간다
	FWarriorStageResumeInfo Resume;
	Resume.StagePlayId = TEXT("f07abfd7-ba7f-4ddb-a4d7-47e7b75dc3f5");
	Resume.ResumeFromWave = 4;
	Resume.WavePlan.Difficulty = 1;
	Resume.WavePlan.Waves.AddDefaulted();
	Launch->SelectStageForResume(StageId, Resume);
	TestEqual(TEXT("Resume uses the play's difficulty"), Launch->GetLaunchInfo().Difficulty, 1);
	TestEqual(TEXT("Screen selection is unchanged"), Launch->GetSelectedDifficulty(), 0);

	// 이어하기 뒤 같은 스테이지를 보통으로 고르면 화면 선택(0)으로 다시 만든다
	Launch->SelectStage(StageId);
	TestEqual(TEXT("Normal selection after resume"), Launch->GetLaunchInfo().Difficulty, 0);
	FWarriorStageResumeInfo Consumed;
	TestFalse(TEXT("Normal selection clears the resume"), Launch->ConsumeResumeInfo(StageId, Consumed));

	// 서버 판 없는 디버그 이어하기(구성 없음)는 화면 선택을 쓴다
	Launch->SetSelectedDifficulty(2);
	FWarriorStageResumeInfo DebugResume;
	DebugResume.ResumeFromWave = 2;
	Launch->SelectStageForResume(StageId, DebugResume);
	TestEqual(TEXT("Debug resume uses the screen selection"), Launch->GetLaunchInfo().Difficulty, 2);
	return true;
}

#endif
