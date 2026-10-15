#include "WarriorStageLaunchSubsystem.h"
#include "WarriorStageCatalogDataAsset.h"
#include "WarriorStageSettings.h"
#include "WarriorStageWaveDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ProjectWarrior/ProjectWarrior.h"

namespace WarriorStageLaunch_Private
{
	void FillFromEntry(const FWarriorStageCatalogEntry& InEntry, FWarriorStageLaunchInfo& OutInfo)
	{
		OutInfo.StageId = InEntry.StageId;
		OutInfo.RegionId = InEntry.RegionId;
		OutInfo.Level = InEntry.Level;
#if WITH_EDITORONLY_DATA
		//개발용 로컬 웨이브는 에디터에서만(쿠킹한 빌드에는 없음, M24)
		OutInfo.WaveData = InEntry.WaveData;
#endif
		OutInfo.bFromCatalog = true;
	}
}

UWarriorStageLaunchSubsystem* UWarriorStageLaunchSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWarriorStageLaunchSubsystem>() : nullptr;
}

void UWarriorStageLaunchSubsystem::SelectStage(FName InStageId)
{
	//보통 선택은 새 플레이로 시작한다
	PendingResume = FWarriorStageResumeInfo();

	if (InStageId.IsNone())
	{
		ClearSelection();
		return;
	}

	//같은 스테이지·같은 난이도면 그대로. 난이도만 바꾼 재입장은 아래에서 다시 만든다(M9)
	if (LaunchInfo.StageId == InStageId && LaunchInfo.Difficulty == SelectedDifficulty)
	{
		return;
	}

	const bool bFromCatalog = BuildLaunchInfo(InStageId, LaunchInfo);
	LaunchInfo.Difficulty = SelectedDifficulty;
	if (!bFromCatalog)
	{
		//임시: 목록에 없는 키도 그대로 쓴다(서버가 판 시작에서 STAGE_NOT_FOUND로 거절한다).
		//게임 파일 검사(세션 연결 뒤 서버 정의와 목록 대조)가 생기면 그 단계에서 막는다. 인계: stage-p4-handover-1009.md
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Selected stage %s is not in the stage catalog. The level's own waves will be used."), *InStageId.ToString());
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Stage selected: %s difficulty %d (level %s, waves %s)"),
		*LaunchInfo.StageId.ToString(),
		LaunchInfo.Difficulty,
		LaunchInfo.Level.IsNull() ? TEXT("-") : *LaunchInfo.Level.ToSoftObjectPath().GetLongPackageName(),
		LaunchInfo.WaveData.IsNull() ? TEXT("level") : *LaunchInfo.WaveData.ToSoftObjectPath().GetAssetName());

	OnLaunchInfoChanged.Broadcast(LaunchInfo);
}

void UWarriorStageLaunchSubsystem::SetSelectedDifficulty(int32 InDifficulty)
{
	SelectedDifficulty = FMath::Max(0, InDifficulty);
}

void UWarriorStageLaunchSubsystem::SelectStageForResume(FName InStageId, const FWarriorStageResumeInfo& InResume)
{
	SelectStage(InStageId);
	if (InStageId.IsNone())
	{
		return;
	}

	PendingResume = InResume;
	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Stage %s will resume play %s from wave %d"),
		*InStageId.ToString(), *PendingResume.StagePlayId, PendingResume.ResumeFromWave);
}

bool UWarriorStageLaunchSubsystem::ConsumeResumeInfo(FName InStageId, FWarriorStageResumeInfo& OutResume)
{
	if (!PendingResume.IsSet() || LaunchInfo.StageId != InStageId)
	{
		return false;
	}

	OutResume = PendingResume;
	PendingResume = FWarriorStageResumeInfo();
	return true;
}

void UWarriorStageLaunchSubsystem::ClearSelection()
{
	PendingResume = FWarriorStageResumeInfo();

	if (!LaunchInfo.IsSet())
	{
		return;
	}

	LaunchInfo = FWarriorStageLaunchInfo();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Stage selection cleared"));

	OnLaunchInfoChanged.Broadcast(LaunchInfo);
}

bool UWarriorStageLaunchSubsystem::BuildLaunchInfo(FName InStageId, FWarriorStageLaunchInfo& OutInfo)
{
	OutInfo = FWarriorStageLaunchInfo();
	OutInfo.StageId = InStageId;

	const UWarriorStageCatalogDataAsset* Catalog = UWarriorStageSettings::LoadStageCatalog();
	const FWarriorStageCatalogEntry* Entry = Catalog ? Catalog->FindEntry(InStageId) : nullptr;
	if (!Entry)
	{
		return false;
	}

	WarriorStageLaunch_Private::FillFromEntry(*Entry, OutInfo);
	return true;
}

bool UWarriorStageLaunchSubsystem::BuildLaunchInfoForLevel(const FString& InLevelPackageName, FWarriorStageLaunchInfo& OutInfo)
{
	OutInfo = FWarriorStageLaunchInfo();

	const UWarriorStageCatalogDataAsset* Catalog = UWarriorStageSettings::LoadStageCatalog();
	const FWarriorStageCatalogEntry* Entry = Catalog ? Catalog->FindFirstEntryByLevel(InLevelPackageName) : nullptr;
	if (!Entry)
	{
		return false;
	}

	WarriorStageLaunch_Private::FillFromEntry(*Entry, OutInfo);
	return true;
}
