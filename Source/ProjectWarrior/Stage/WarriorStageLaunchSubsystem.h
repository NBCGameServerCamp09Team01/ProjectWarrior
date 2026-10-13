#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorStageLaunchSubsystem.generated.h"

class UWarriorStageWaveDataAsset;
class UWorld;

/** 한 판을 시작할 때 정해져 레벨이 끝날 때까지 바뀌지 않는 스테이지 정보. */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageLaunchInfo
{
	GENERATED_BODY()

	//서버 stageId. 결과·판 시작에 그대로 실린다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName StageId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	TSoftObjectPtr<UWorld> Level;

	//비어 있으면 레벨의 FlowManager 웨이브를 쓴다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	TSoftObjectPtr<UWarriorStageWaveDataAsset> WaveData;

	//스테이지 목록에서 찾은 줄로 만들었으면 true. false면 StageId만 있다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bFromCatalog = false;

	bool IsSet() const { return !StageId.IsNone(); }
};

/**
 * 진행 중인 서버 플레이를 이어 갈 때의 정보(stage-play-api.md "이어하기(1차)").
 * 1차는 늘 처음(1웨이브)부터 다시 한다. 웨이브 중간부터 이어 가는 단계에서 체크포인트 값(골드·인벤토리·판 강화 등)을 여기에 더한다.
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorStageResumeInfo
{
	GENERATED_BODY()

	//이어 갈 서버 플레이 ID. 비어 있으면 이어하기가 아니다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FString StagePlayId;

	//그 플레이의 마감(UTC ISO-8601). 결과를 보관할 때 유효 기간으로 쓴다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FString ExpiresAt;

	//이어서 시작할 웨이브(1부터). 1차는 늘 1
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 ResumeFromWave = 1;

	bool IsSet() const { return !StagePlayId.IsEmpty(); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorStageLaunchInfoChanged, const FWarriorStageLaunchInfo&, LaunchInfo);

/**
 * 다음에 플레이할 스테이지를 정하고, 스테이지 목록으로 레벨·웨이브를 찾아 실행 정보로 들고 있다.
 * GameInstance 서브시스템이라 레벨을 옮겨도(메인메뉴 → 스테이지 → 다시 하기) 값이 남는다.
 * 스테이지 GameMode가 레벨 시작 때 GetLaunchInfo()를 읽는다. 진행(열림·잠김)은 이 클래스가 맡지 않는다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageLaunchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStageLaunchSubsystem* Get(const UObject* WorldContextObject);

	//다음에 시작할 스테이지를 정한다. 스테이지 선택 화면에서 호출한다. None이면 선택을 지운다.
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void SelectStage(FName InStageId);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void ClearSelection();

	//진행 중인 서버 플레이를 이어 가도록 스테이지를 고른다. 다음 스테이지 레벨의 BeginRun이 ConsumeResumeInfo로 한 번만 가져간다
	void SelectStageForResume(FName InStageId, const FWarriorStageResumeInfo& InResume);

	//InStageId의 이어하기 정보가 있으면 꺼내고 지운다(결과 화면의 "다시 하기"가 끝난 플레이 ID를 다시 쓰지 않게). 없거나 스테이지가 다르면 false
	bool ConsumeResumeInfo(FName InStageId, FWarriorStageResumeInfo& OutResume);

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	bool HasLaunchInfo() const { return LaunchInfo.IsSet(); }

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	const FWarriorStageLaunchInfo& GetLaunchInfo() const { return LaunchInfo; }

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorStageLaunchInfoChanged OnLaunchInfoChanged;

	//스테이지 키로 실행 정보를 만든다. OutInfo.StageId는 늘 InStageId다. 목록에서 찾았으면 true
	static bool BuildLaunchInfo(FName InStageId, FWarriorStageLaunchInfo& OutInfo);

	//Level이 이 레벨 패키지인 목록 첫 줄로 실행 정보를 만든다. 없으면 false (OutInfo는 비움)
	static bool BuildLaunchInfoForLevel(const FString& InLevelPackageName, FWarriorStageLaunchInfo& OutInfo);

private:
	FWarriorStageLaunchInfo LaunchInfo;

	//SelectStageForResume으로 정한 이어하기 정보. 다른 스테이지를 고르거나 선택을 지우면 함께 지운다
	FWarriorStageResumeInfo PendingResume;
};
