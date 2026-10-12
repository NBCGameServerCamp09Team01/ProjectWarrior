#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WarriorStageCatalogDataAsset.generated.h"

class UWarriorStageWaveDataAsset;
class UWorld;

/** 스테이지 목록 한 줄. 서버의 스테이지 키와 실제 레벨·웨이브를 잇는다. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageCatalogEntry
{
	GENERATED_BODY()

	//서버 master/stages.json의 stageId와 같은 문자열 (예: stage.01.01). 소문자·숫자·'.'만 쓰고 바꾸지 않는다. 레벨 이름이 아니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage")
	FName StageId;

	//서버의 regionId (예: region.01). 선택 화면이 지역으로 묶을 때 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage")
	FName RegionId;

	//이 스테이지를 플레이할 레벨. 여러 스테이지가 같은 레벨을 써도 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage")
	TSoftObjectPtr<UWorld> Level;

	//비어 있으면 레벨의 FlowManager에 둔 웨이브를 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage")
	TSoftObjectPtr<UWarriorStageWaveDataAsset> WaveData;
};

/**
 * 스테이지 목록. 서버 스테이지 키 → 레벨·웨이브 DataAsset.
 * 화면 이름·순서·해금은 두지 않는다(서버가 준다). 경로는 프로젝트 설정 > Game > Warrior Stage.
 */
UCLASS(BlueprintType)
class PROJECTWARRIOR_API UWarriorStageCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	//없으면 nullptr
	const FWarriorStageCatalogEntry* FindEntry(FName InStageId) const;

	//목록 첫 줄. 비어 있으면 nullptr
	const FWarriorStageCatalogEntry* GetFirstEntry() const;

	//Level이 이 패키지(예: /Game/MyGameContents/Maps/Test/L_Stage_Test)인 첫 줄. 없으면 nullptr
	const FWarriorStageCatalogEntry* FindFirstEntryByLevel(const FString& InLevelPackageName) const;

	const TArray<FWarriorStageCatalogEntry>& GetStages() const { return Stages; }

	//서버 키 규칙: 소문자·숫자·'.', 1~64자
	static bool IsValidStageKey(FName InKey);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Stage", meta = (AllowPrivateAccess = "true", TitleProperty = "StageId"))
	TArray<FWarriorStageCatalogEntry> Stages;
};
