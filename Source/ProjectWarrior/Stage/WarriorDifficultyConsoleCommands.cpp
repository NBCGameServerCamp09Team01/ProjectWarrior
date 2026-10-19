#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"

/**
 * 난이도(M9) 시험용 콘솔 명령. 선택 팝업 WBP에 난이도 버튼(Button_Difficulty)이 붙기 전에 PIE에서 쓴다.
 *   Warrior.Stage.Difficulty        지금 고른 난이도를 로그에 남긴다
 *   Warrior.Stage.Difficulty 2      다음 입장부터 난이도 2(하드)로 시작 요청을 보낸다. 서버에 없는 번호면 판 시작이 400으로 막힌다
 */
namespace WarriorDifficultyConsole
{
	void SetDifficulty(const TArray<FString>& Args, UWorld* World)
	{
		UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(World);
		if (!LaunchSubsystem)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Difficulty command ignored. No stage launch subsystem."));
			return;
		}

		if (Args.Num() > 0)
		{
			LaunchSubsystem->SetSelectedDifficulty(FCString::Atoi(*Args[0]));
		}
		UE_LOG(LogProjectWarrior, Display, TEXT("[Stage] Selected difficulty %d (applies from the next stage entry)."),
			LaunchSubsystem->GetSelectedDifficulty());
	}

	FAutoConsoleCommandWithWorldAndArgs DifficultyCommand(
		TEXT("Warrior.Stage.Difficulty"), TEXT("다음 입장의 난이도를 고른다(0 이지, 1 노말, 2 하드). 인자 없으면 지금 값을 보인다(디버그용, M9)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetDifficulty));
}

#endif // !UE_BUILD_SHIPPING
