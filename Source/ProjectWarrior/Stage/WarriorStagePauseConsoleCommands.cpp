#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorStagePlayerController.h"

/**
 * 일시정지 디버그용 콘솔 명령.
 *   Warrior.Stage.Pause    일시정지를 열고 닫는다(열 수 없으면 [Pause] Blocked 로그로 이유를 남긴다)
 * PIE에서는 ESC를 에디터가 가져가(PIE 종료) 일시정지 키로 열 수 없으므로 이 명령으로 연다.
 * 실제 경로(AWarriorStagePlayerController::TryOpenPause·ClosePause)를 그대로 부른다.
 */
namespace WarriorStagePauseConsole
{
	void TogglePause(const TArray<FString>& Args, UWorld* World)
	{
		AWarriorStagePlayerController* StagePlayerController = World ? Cast<AWarriorStagePlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!StagePlayerController)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Pause] Pause command ignored. Not in a stage level."));
			return;
		}

		if (StagePlayerController->IsPauseOpen())
		{
			StagePlayerController->ClosePause();
		}
		else
		{
			StagePlayerController->TryOpenPause();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs PauseCommand(
		TEXT("Warrior.Stage.Pause"), TEXT("일시정지를 열고 닫는다. 열 수 없으면 이유를 로그로 남긴다(디버그용)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TogglePause));
}

#endif // !UE_BUILD_SHIPPING
