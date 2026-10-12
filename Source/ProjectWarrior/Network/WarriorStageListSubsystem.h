// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorStageListTypes.h"
#include "WarriorStageListSubsystem.generated.h"

struct FWarriorStageLaunchInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorStageListUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorStageListFailed, const FString&, ErrorCode, const FString&, ErrorMessage);

/**
 * 스테이지 선택 팝업이 쓰는 목록을 서버에서 받아 들고 있다.
 * 명세: 루트 docs/contracts/stage-api.md v1.1 (ST1 GET /stages, ST2 GET /accounts/me/stages)
 *
 * - RequestStageList: ST1은 처음 한 번 받아 두고, ST2는 부를 때마다 받는다(팝업을 열 때마다). 둘을 stageId로 합쳐 OnStageListUpdated.
 * - 열림·잠김은 서버 status대로만 그린다. 게임에 해금 규칙을 두지 않는다.
 * - 서버엔 있고 UE 스테이지 목록(DA_StageCatalog)에 없는 스테이지는 목록에서 빼고 경고 로그를 남긴다(입장할 레벨이 없음).
 * - 웨이브 구성은 지금 UE 웨이브 DataAsset에서 읽는다. 서버가 보내게 되면 BuildWaveInfo 한 곳만 바꾼다.
 * - 로그인 상태가 끝나면(OnSessionEnded) 진행 사본과 스테이지 선택값을 비운다(다른 계정이 이어받지 않게).
 * - 요청은 UWarriorAuthSubsystem::SendAuthorized로 보낸다. 실패는 로그 + OnStageListFailed까지(안내 문구는 b의 S5 오류 문구 표).
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStageListSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStageListSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//정의(처음 한 번)와 내 진행을 받아 목록을 다시 만든다. 받는 중에 다시 부르면 앞 요청의 응답은 버린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RequestStageList();

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	bool IsRequesting() const { return bRequesting; }

	//한 번이라도 목록을 만들었는가
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	bool HasStageList() const { return bHasList; }

	//order 순 지역 목록
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	const TArray<FWarriorRegionListEntry>& GetRegions() const { return Regions; }

	//목록에서 스테이지 한 줄. 없으면 nullptr
	const FWarriorStageListEntry* FindStage(FName InStageId) const;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorStageListUpdated OnStageListUpdated;

	//ErrorCode는 서버 오류 code(또는 NETWORK_ERROR, HTTP_<상태>, INVALID_RESPONSE). 401은 Auth가 이미 로그인 상태를 끝냈다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorStageListFailed OnStageListFailed;

private:
	//── 정의 한 줄(ST1)
	struct FStageDef
	{
		FName StageId;
		FString Name;
		int32 Order = 0;
		FName RequiresStageId;
		int32 WaveCount = 0;
	};

	struct FRegionDef
	{
		FName RegionId;
		FString Name;
		int32 Order = 0;
		TArray<FStageDef> Stages;
	};

	//── 진행 한 줄(ST2)
	struct FStageProgress
	{
		EWarriorStageEntryStatus Status = EWarriorStageEntryStatus::Locked;
		FDateTime FirstClearedAt;
		bool bHasFirstCleared = false;
	};

	void SendDefinitions(int32 InSerial);
	void SendProgress(int32 InSerial);
	void HandleDefinitionsResponse(int32 InSerial, int32 Status, const FString& Body);
	void HandleProgressResponse(int32 InSerial, int32 Status, const FString& Body);

	bool ParseDefinitions(const FString& InBody);
	bool ParseProgress(const FString& InBody);

	//정의 + 진행 + UE 스테이지 목록으로 Regions를 다시 만든다
	void RebuildRegions();

	void Fail(int32 InSerial, int32 Status, const FString& Body);

	//지역을 열 수 있는가. 지금은 늘 열림. 서버가 지역 상태를 보내거나 규칙(안에 열린 스테이지가 없으면 잠김)이 생기면 여기만 바꾼다
	static bool IsRegionOpen(const FWarriorRegionListEntry& InRegion);

	//웨이브 구성을 채운다. 지금 출처는 UE 스테이지 목록의 웨이브 DataAsset.
	//서버가 ST1에 웨이브 구성을 보내게 되면 이 함수가 서버 값을 쓰도록 바꾼다(호출하는 쪽은 그대로)
	static void BuildWaveInfo(const FWarriorStageLaunchInfo& InLaunchInfo, int32 InServerWaveCount, TArray<FWarriorStageWaveInfo>& OutWaves);

	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage);

	bool IsCurrentRequest(int32 InSerial) const { return InSerial == RequestSerial; }

	TArray<FRegionDef> RegionDefs;
	bool bHasDefinitions = false;

	TMap<FName, FStageProgress> Progress;

	UPROPERTY(Transient)
	TArray<FWarriorRegionListEntry> Regions;

	bool bHasList = false;
	bool bRequesting = false;

	//RequestStageList·세션 끝마다 오른다. 늦게 온 지난 요청의 응답을 버린다
	int32 RequestSerial = 0;
};
