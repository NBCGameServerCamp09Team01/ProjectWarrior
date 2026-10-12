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
		OutInfo.WaveData = InEntry.WaveData;
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
	if (InStageId.IsNone())
	{
		ClearSelection();
		return;
	}

	if (LaunchInfo.StageId == InStageId)
	{
		return;
	}

	if (!BuildLaunchInfo(InStageId, LaunchInfo))
	{
		//임시: 목록에 없는 키도 그대로 쓴다(서버가 판 시작에서 STAGE_NOT_FOUND로 거절한다).
		//게임 파일 검사(세션 연결 뒤 서버 정의와 목록 대조)가 생기면 그 단계에서 막는다. 인계: stage-p4-handover-1009.md
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Stage] Selected stage %s is not in the stage catalog. The level's own waves will be used."), *InStageId.ToString());
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Stage] Stage selected: %s (level %s, waves %s)"),
		*LaunchInfo.StageId.ToString(),
		LaunchInfo.Level.IsNull() ? TEXT("-") : *LaunchInfo.Level.ToSoftObjectPath().GetLongPackageName(),
		LaunchInfo.WaveData.IsNull() ? TEXT("level") : *LaunchInfo.WaveData.ToSoftObjectPath().GetAssetName());

	OnLaunchInfoChanged.Broadcast(LaunchInfo);
}

void UWarriorStageLaunchSubsystem::ClearSelection()
{
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
