#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorStagePlayerController.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"

/**
 * 일시정지·나가기 시험용 콘솔 명령. 일시정지 화면(ESC·버튼)이 붙기 전에 쓴다(일시정지·나가기 계획서 1·3단계).
 *   Warrior.Stage.Leave    지금 판을 포기(P4)하고, 응답을 받은 뒤 메인메뉴로 간다. 실패면 안내 팝업을 띄우고 간다
 *   Warrior.Stage.Pause    일시정지를 열고 닫는다(열 수 없으면 [Pause] Blocked 로그)
 * 화면이 붙으면 같은 흐름을 컨트롤러·위젯이 하므로 이 명령들은 지운다.
 */
namespace WarriorStageLeaveConsole
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

	void Leave(const TArray<FString>& Args, UWorld* World)
	{
		AWarriorStagePlayerController* StagePlayerController = World ? Cast<AWarriorStagePlayerController>(World->GetFirstPlayerController()) : nullptr;
		UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(World);
		if (!StagePlayerController || !StagePlay)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Leave command ignored. Not in a stage level."));
			return;
		}

		UE_LOG(LogProjectWarrior, Display, TEXT("[Stage] Leave command. stagePlayId=%s"), *StagePlay->GetCurrentStagePlayId());

		//응답 전에 레벨이 바뀔 수 있으므로(세션 끝 등) 컨트롤러는 약한 참조로 잡는다
		StagePlay->AbandonCurrentStagePlay([WeakController = TWeakObjectPtr<AWarriorStagePlayerController>(StagePlayerController)](bool bEnded)
		{
			AWarriorStagePlayerController* Controller = WeakController.Get();
			if (!Controller)
			{
				UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Leave finished after the stage level was gone. ended=%s"), bEnded ? TEXT("true") : TEXT("false"));
				return;
			}

			UE_LOG(LogProjectWarrior, Display, TEXT("[Stage] Leave finished. ended=%s"), bEnded ? TEXT("true") : TEXT("false"));

			//포기하지 못했으면 안내한다. 팝업은 레벨이 바뀌어도 메인메뉴에 다시 뜬다(UWarriorErrorPresenterSubsystem)
			if (!bEnded)
			{
				if (UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(Controller))
				{
					FWarriorApiResult Result;
					Result.bOk = false;
					Result.Error.Code = TEXT("STAGE_PLAY_ABANDON_FAILED");
					Result.Rule = FWarriorErrorText::Resolve(Result.Error);
					Api->PresentError(Result);
				}
			}
			Controller->ReturnToMainMenu();
		});
	}

	FAutoConsoleCommandWithWorldAndArgs LeaveCommand(
		TEXT("Warrior.Stage.Leave"), TEXT("지금 판을 포기(P4)하고 메인메뉴로 간다. 일시정지 → 나가기의 시험용(디버그용)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Leave));
}

#endif // !UE_BUILD_SHIPPING
