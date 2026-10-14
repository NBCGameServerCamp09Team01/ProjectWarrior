#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/GameModes/WarriorStageGameMode.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"
#include "ProjectWarrior/Stage/WarriorPlayCheckpoint.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"

/**
 * 플레이 체크포인트(M22 최종 1단계) 시험용 콘솔 명령. 스테이지 PIE 중 ~ 콘솔에서 입력한다.
 *   Warrior.Checkpoint.Capture   지금 판 안 값을 꺼내 JSON으로 바꿨다가 다시 읽어 보관한다(서버 왕복과 같은 모양)
 *   Warrior.Checkpoint.Restore   보관한 값을 넣는다
 *   Warrior.Checkpoint.Dump      보관한 값을 JSON으로 로그에 남긴다
 *   Warrior.Checkpoint.ResumeLevel  보관한 값으로 지금 스테이지를 다시 열어 다음 웨이브의 쉬는 시간부터 시작한다(2단계)
 * 보관 값은 이 프로세스 안에서만 남는다(레벨을 옮겨도 남음).
 */
namespace WarriorPlayCheckpointConsole
{
	FWarriorPlayCheckpoint Stored;
	bool bHasStored = false;

	void Capture(const TArray<FString>& Args, UWorld* World)
	{
		FWarriorPlayCheckpoint Captured;
		FString Error;
		if (!WarriorPlayCheckpoint::Capture(World, Captured, Error))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Debug capture failed: %s"), *Error);
			return;
		}

		//서버로 오갈 모양 그대로 보관한다. JSON에서 빠지는 칸이 있으면 여기서 드러난다
		FString Json;
		FWarriorPlayCheckpoint RoundTrip;
		if (!WarriorPlayCheckpoint::ToJson(Captured, Json) || !WarriorPlayCheckpoint::FromJson(Json, RoundTrip))
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[PlayCheckpoint] Debug capture JSON round trip failed."));
			return;
		}

		Stored = MoveTemp(RoundTrip);
		bHasStored = true;
		UE_LOG(LogProjectWarrior, Display, TEXT("[PlayCheckpoint] Debug captured (json %d chars). %s"), Json.Len(), *WarriorPlayCheckpoint::Describe(Stored));
	}

	void Restore(const TArray<FString>& Args, UWorld* World)
	{
		if (!bHasStored)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Debug restore ignored. Run Warrior.Checkpoint.Capture first."));
			return;
		}

		FString Error;
		if (!WarriorPlayCheckpoint::Restore(World, Stored, Error))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Debug restore failed: %s"), *Error);
		}
	}

	void Dump(const TArray<FString>& Args, UWorld* World)
	{
		FString Json;
		if (!bHasStored || !WarriorPlayCheckpoint::ToJson(Stored, Json))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Nothing to dump. Run Warrior.Checkpoint.Capture first."));
			return;
		}
		UE_LOG(LogProjectWarrior, Display, TEXT("[PlayCheckpoint] Debug checkpoint json: %s"), *Json);
	}

	//보관한 값으로 지금 스테이지 레벨을 다시 연다. 실제 이어하기와 같은 길(예약 → BeginRun)로 다음 웨이브의 쉬는 시간부터 시작한다
	void ResumeLevel(const TArray<FString>& Args, UWorld* World)
	{
		const AWarriorStageGameMode* StageGameMode = World ? World->GetAuthGameMode<AWarriorStageGameMode>() : nullptr;
		UWarriorStageLaunchSubsystem* LaunchSubsystem = UWarriorStageLaunchSubsystem::Get(World);
		if (!bHasStored || !StageGameMode || !LaunchSubsystem)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[PlayCheckpoint] Debug resume ignored. Needs a stage level and Warrior.Checkpoint.Capture first."));
			return;
		}

		//서버 플레이가 있으면 같은 ID로 이어 간다(없으면 서버 없이)
		//M24: 그 판의 웨이브 구성(시작 때 고정)도 함께 넘긴다. 없으면 이어한 판이 STAGE_WAVE_PLAN_MISSING으로 시작하지 않는다
		FWarriorStageResumeInfo ResumeInfo;
		if (const UWarriorStagePlaySubsystem* StagePlay = UWarriorStagePlaySubsystem::Get(World))
		{
			ResumeInfo.StagePlayId = StagePlay->GetCurrentStagePlayId();
			ResumeInfo.WavePlan = StagePlay->GetWavePlan();
		}
		ResumeInfo.ResumeFromWave = Stored.ClearedWave + 1;
		ResumeInfo.Checkpoint = Stored;

		LaunchSubsystem->SelectStageForResume(StageGameMode->GetResolvedStageId(), ResumeInfo);

		const FString LevelName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		UE_LOG(LogProjectWarrior, Display, TEXT("[PlayCheckpoint] Debug resume: reopening %s from wave %d."), *LevelName, ResumeInfo.ResumeFromWave);
		UGameplayStatics::OpenLevel(World, FName(*LevelName));
	}

	FAutoConsoleCommandWithWorldAndArgs ResumeLevelCommand(
		TEXT("Warrior.Checkpoint.ResumeLevel"), TEXT("보관한 판 안 값으로 지금 스테이지를 다시 열어 다음 웨이브의 쉬는 시간부터 시작한다(디버그용, M22)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResumeLevel));

	FAutoConsoleCommandWithWorldAndArgs CaptureCommand(
		TEXT("Warrior.Checkpoint.Capture"), TEXT("판 안 값을 꺼내 보관한다(디버그용, M22)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Capture));

	FAutoConsoleCommandWithWorldAndArgs RestoreCommand(
		TEXT("Warrior.Checkpoint.Restore"), TEXT("보관한 판 안 값을 넣는다(디버그용, M22)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Restore));

	FAutoConsoleCommandWithWorldAndArgs DumpCommand(
		TEXT("Warrior.Checkpoint.Dump"), TEXT("보관한 판 안 값을 JSON으로 로그에 남긴다(디버그용, M22)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
