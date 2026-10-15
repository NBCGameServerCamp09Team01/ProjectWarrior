// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWarrior/Network/WarriorStageListTypes.h"
#include "WarriorStageSelectWidgets.generated.h"

class UButton;
class UHorizontalBox;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UWarriorStageRankingCardWidget;

/**
 * 스테이지 선택 팝업의 위젯 넷(팝업·지역 칸·스테이지 막대·웨이브 줄).
 * 목록은 UWarriorStageListSubsystem(서버 ST1·ST2)에서 오고, 위젯은 받은 값을 그리기만 한다.
 * 버튼·글자는 BindWidgetOptional이라 WBP에 같은 이름으로 두면 붙는다. 색·여백·모양은 WBP가 맡는다
 * (기존 메인메뉴 버튼 스타일을 그대로 쓴다. 도안: docs_local/part-d/ui/stage-select-draft-1011.html).
 */

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWarriorStageSelectIdClicked, FName /*Id*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorStageSelectPopupClosed);

//팝업이 보이는 것: 받는 중 / 목록 / 받기 실패
enum class EWarriorStageSelectViewState : uint8
{
	Loading,
	List,
	Error
};

/** 웨이브 한 줄: "웨이브 1" · "BP_SamuraiAI × 3 · BP_ArcherAI × 1". */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorWaveEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorWaveEntryWidget(const FObjectInitializer& ObjectInitializer);

	//InDifficulty: 선택 화면에서 고른 난이도(M9). 그 난이도의 마리 수(ST1 counts)를 보인다
	void SetWave(const FWarriorStageWaveInfo& InWave, int32 InDifficulty = 0);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WaveLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Enemies;

	//{0} = 웨이브 번호
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText WaveLabelFormat;

	//{0} = 몬스터 키, {1} = 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText EnemyFormat;

	//보스 몬스터 뒤에 붙는 글자
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText BossSuffix;
};

/** 스테이지 막대 하나. 막대를 누르면 아래로 웨이브 정보가 펼쳐지고, "입장"만 스테이지로 들어간다. */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorStageEntryWidget(const FObjectInitializer& ObjectInitializer);

	void SetStage(const FWarriorStageListEntry& InStage, int32 InDifficulty = 0);

	//고른 난이도가 바뀌면(M9) 펼친 웨이브 줄의 마리 수를 다시 그린다
	void SetDifficulty(int32 InDifficulty);

	//입장 버튼. 인자는 StageId
	FOnWarriorStageSelectIdClicked OnEnterClicked;

	//순위 버튼(입장 왼쪽, 랭킹 M7). 인자는 StageId
	FOnWarriorStageSelectIdClicked OnRankingClicked;

	//입장 요청을 보내는 동안 버튼을 막는다
	void SetInteractionLocked(bool bInLocked);

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//상태(열림·클리어·잠김)가 정해질 때. 색은 C++이 라벨에만 넣고, 그 밖의 모양은 BP에서 바꾼다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Stage", meta = (DisplayName = "On Stage Status Set"))
	void BP_OnStageStatusSet(EWarriorStageEntryStatus InStatus);

	UFUNCTION()
	void HandleBarClicked();

	UFUNCTION()
	void HandleEnterClicked();

	UFUNCTION()
	void HandleRankingClicked();

	void SetWavesExpanded(bool bInExpanded);
	void ApplyStatusLook();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Bar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Enter;

	//순위(입장 왼쪽). 잠긴 스테이지는 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Ranking;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StageInfo;

	//웨이브 줄을 넣는 칸(ScrollBox 권장). 펼치기 전에는 접혀 있다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ScrollBox_Waves;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorWaveEntryWidget> WaveEntryClass;

	//{0} = 지역 안 순서
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText StageLabelFormat;

	//{0} = 웨이브 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText WaveCountFormat;

	//{0} = 웨이브 수, {1} = 처음 클리어 날짜(월/일)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText ClearedInfoFormat;

	//상태는 색으로만 보인다(글자로 "잠김"을 쓰지 않음). 라벨 색과 잠김 투명도
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	FSlateColor OpenLabelColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	FSlateColor ClearedLabelColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	FSlateColor LockedLabelColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockedOpacity = 0.45f;

private:
	FWarriorStageListEntry Stage;
	bool bWavesExpanded = false;
	bool bWavesBuilt = false;
	bool bInteractionLocked = false;

	//웨이브 줄에 보일 난이도(M9)
	int32 Difficulty = 0;
};

/** 지역 칸 하나: 이름 + 스테이지마다 점(클리어한 수만큼 채움). */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorRegionEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRegion(const FWarriorRegionListEntry& InRegion, bool bInSelected);

	//인자는 RegionId
	FOnWarriorStageSelectIdClicked OnRegionClicked;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//선택됨·열림이 정해질 때. 선택 테두리·잠김 모양(확장)은 BP에서 그린다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Stage", meta = (DisplayName = "On Region State Set"))
	void BP_OnRegionStateSet(bool bInSelected, bool bInCanOpen);

	UFUNCTION()
	void HandleRegionClicked();

	void RebuildPips(const FWarriorRegionListEntry& InRegion);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Region;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RegionName;

	//진행 점을 넣는 칸
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Pips;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	FLinearColor PipClearedColor = FLinearColor(0.58f, 0.36f, 0.10f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	FLinearColor PipEmptyColor = FLinearColor(0.068f, 0.014f, 0.009f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	FVector2D PipSize = FVector2D(14.f, 14.f);

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Look")
	float PipSpacing = 6.f;

private:
	FName RegionId;
	bool bCanOpen = true;
};

/**
 * 스테이지 선택 팝업. 메인메뉴 위에 뜬다(WBP_NoticePopup과 같은 틀: 화면 덮기 + 가운데 카드).
 * 프론트 컨트롤러가 OpenStageSelect에서 띄우고 Refresh를 부른다. 입장은 프론트 컨트롤러의 EnterStage로 넘긴다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageSelectPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorStageSelectPopupWidget(const FObjectInitializer& ObjectInitializer);

	//목록을 다시 받는다(팝업을 열 때마다)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void Refresh();

	//팝업을 닫고 OnClosed로 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void Close();

	//입장 요청이 실패했을 때(프론트 컨트롤러가 부른다). 막았던 버튼을 풀고 목록을 다시 받는다
	void HandleEnterFailed();

	//난이도를 고른다(M9). 다음 입장의 판 시작 요청에 실리고(UWarriorStageLaunchSubsystem), 웨이브 미리보기 마리 수가 바뀐다.
	//서버 목록에 없는 번호면 0(이지)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void SetDifficulty(int32 InDifficulty);

	//지금 고른 난이도. 게임을 켜 둔 동안 기억한다(처음 0 = 이지)
	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	int32 GetDifficulty() const;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Stage")
	FOnWarriorStageSelectPopupClosed OnClosed;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleListUpdated();

	UFUNCTION()
	void HandleListFailed(const FString& InErrorCode, const FString& InErrorMessage);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleRetryClicked();

	//난이도 버튼: 다음 난이도로(이지 → 노말 → 하드 → 이지)
	UFUNCTION()
	void HandleDifficultyClicked();

	//난이도 글자를 지금 값으로 맞추고 BP에 알린다
	void UpdateDifficultyLabel();

	//난이도가 바뀌었을 때(WBP가 버튼 모양을 바꾸는 데 씀)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Stage", meta = (DisplayName = "On Difficulty Changed"))
	void BP_OnDifficultyChanged(int32 InDifficulty, const FText& InDisplayName);

	void HandleRegionClicked(FName InRegionId);
	void HandleStageEnterClicked(FName InStageId);
	void HandleStageRankingClicked(FName InStageId);

	UFUNCTION()
	void HandleStageRankingCardClosed();

	void SetViewState(EWarriorStageSelectViewState InState);

	void RebuildRegions();
	void RebuildStages();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HorizontalBox_Regions;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ScrollBox_Stages;

	//받는 중 표시
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Loading;

	//받기 실패 표시(안에 Button_Retry). 문구는 b의 오류 문구 표가 생기면 그쪽에서 바꾼다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Error;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Retry;

	//난이도 고르기(M9). WBP에 같은 이름으로 두면 붙는다. 없으면 난이도 0(이지)으로만 들어간다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Difficulty;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Difficulty;

	//{0} = 난이도 이름(서버 ST1 difficulties[].name)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText DifficultyLabelFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorRegionEntryWidget> RegionEntryClass;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorStageEntryWidget> StageEntryClass;

	//스테이지 순위 카드(WBP_StageRankingCard). 이 팝업 위에 한 겹 뜬다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Ranking")
	TSubclassOf<UWarriorStageRankingCardWidget> StageRankingCardClass;

	//카드의 화면 순서. 스테이지 선택 팝업(20)보다 크고 안내 팝업(50)보다 작게
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Ranking")
	int32 StageRankingCardZOrder = 30;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageRankingCardWidget> StageRankingCard;

	//마지막으로 고른 지역. 목록을 다시 받아도 유지한다. 없으면 첫 지역
	FName SelectedRegionId;

	bool bEnterPending = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWarriorStageEntryWidget>> StageEntries;
};
