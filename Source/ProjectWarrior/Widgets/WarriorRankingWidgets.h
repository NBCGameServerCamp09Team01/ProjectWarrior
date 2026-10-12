// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWarrior/Network/WarriorRankingTypes.h"
#include "WarriorRankingWidgets.generated.h"

class UBorder;
class UButton;
class UComboBoxString;
class UListView;
class UPanelWidget;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorRankingViewClosed);

/**
 * 랭킹 화면. 도안: docs_local/part-d/ui/ranking-ui-draft-1012.html(2.1판), 명세: 루트 docs/contracts/ranking-api.md v1
 * - 메인메뉴 "랭킹" → UWarriorRankingPopupWidget: [전체 총합][지역 총합] 탭, 전체·주간, 20줄씩 "더 보기", 내 순위
 * - 스테이지 선택의 "순위" → UWarriorStageRankingCardWidget: 그 스테이지 위 5명 + 내 순위(전체 기간)
 * 버튼·글자는 BindWidgetOptional이라 WBP에 같은 이름으로 두면 붙는다. 색·여백·모양은 WBP가 맡는다.
 */

//ListView 항목(메인 랭킹 팝업). 받은 줄은 이 객체로 메모리에 남고, 화면 줄 위젯은 보이는 만큼만 재사용된다
UCLASS()
class PROJECTWARRIOR_API UWarriorRankingEntryItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FWarriorRankingEntry Entry;

	UPROPERTY()
	EWarriorRankingBoardKind Kind = EWarriorRankingBoardKind::Total;
};

//순위 한 줄(목록 줄·내 줄 같이 씀). ScrollBox에 직접 넣거나 ListView 항목 줄(IUserObjectListEntry)로 쓴다
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorRankingRowWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UWarriorRankingRowWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin IUserObjectListEntry Interface.
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~ End IUserObjectListEntry Interface

	void SetEntry(const FWarriorRankingEntry& InEntry, EWarriorRankingBoardKind InKind);

	//내 기록이 없을 때 내 줄 자리(순위 "-", 문구)
	void SetNoRecord(const FText& InMessage);

protected:
	//줄이 정해질 때. 1~3위 테두리·내 줄 배경은 BP에서(순위 글자 색은 C++이 넣는다)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Ranking", meta = (DisplayName = "On Row Set"))
	void BP_OnRowSet(int32 InRank, bool bInIsMine, bool bInHasRecord);

	virtual void NativeOnInitialized() override;

	//줄 테두리. 1~3위는 금·은·동 테두리, 내 줄은 초록 테두리·배경(도안 2.1판)
	void ApplyRowLook(int32 InRank, bool bInIsMine);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Row;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Rank;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Nickname;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Score;

	//스테이지: 클리어 시간, 총합: "2개 클리어"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Detail;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Date;

	//{0} 점수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText ScoreFormat;

	//{0} 클리어한 스테이지 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText ClearedFormat;

	//내 줄의 닉네임 자리
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText MineName;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FSlateColor GoldColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FSlateColor SilverColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FSlateColor BronzeColor;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FSlateColor RankColor;

	//내 줄 테두리·배경
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FLinearColor MineOutlineColor = FLinearColor(0.117f, 0.319f, 0.144f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FLinearColor MineBackgroundColor = FLinearColor(0.023f, 0.039f, 0.026f, 1.f);

private:
	//WBP에 정한 테두리 브러시·색(보통 줄 모양). 강조를 풀 때 되돌린다
	FSlateBrush DefaultBrush;
	FLinearColor DefaultBrushColor = FLinearColor::White;
	bool bHasDefaultBrush = false;
};

/**
 * 랭킹 보기 공통: 목록 줄 만들기, 내 줄, 불러오는 중·실패 표시, 새로 고침 쿨타임, 닫기.
 * 쿨타임: 성공 뒤 RefreshCooldownAfterSuccess초, 실패 뒤 RefreshCooldownAfterFailure초 동안 새로 고침·다시 불러오기를 막는다(자동으로 다시 받지 않음).
 * 막는 동안은 채도를 빼고 어둡게(InactiveButtonColor) + "새로 고침 (7)".
 * 화면이 떠 있는 동안만 랭킹 서브시스템 알림을 받고, 내가 요청한 보드의 알림만 쓴다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorRankingViewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorRankingViewWidget(const FObjectInitializer& ObjectInitializer);

	//화면을 닫고 OnClosed로 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ranking")
	void Close();

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Ranking")
	FOnWarriorRankingViewClosed OnClosed;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	//지금 보는 보드를 처음부터 다시 받는다(InKind·InTargetId·InPeriod·InLimit을 기억)
	void RequestView(EWarriorRankingBoardKind InKind, FName InTargetId, EWarriorRankingPeriod InPeriod, int32 InLimit);

	//요청 상태가 바뀔 때(불러오는 중·목록·실패·없음). 패널 보이기는 C++이 하고, 그 밖의 모양은 BP에서
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Ranking", meta = (DisplayName = "On View State Set"))
	void BP_OnViewStateSet(bool bInLoading, bool bInFailed, bool bInEmpty);

	//새로 고침 버튼 상태가 바뀔 때(쿨타임 시작·끝, 남은 초)
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Ranking", meta = (DisplayName = "On Refresh State Changed"))
	void BP_OnRefreshStateChanged(bool bInReady, int32 InRemainingSeconds);

	UFUNCTION()
	void HandleBoardUpdated();

	UFUNCTION()
	void HandleBoardFailed(EWarriorRankingFailure InFailure, const FString& InErrorCode);

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleMoreClicked();

	UFUNCTION()
	void HandleCloseClicked();

	//받은 줄을 화면에 옮긴다. bInFirstPage면 처음부터(보드·새로 고침), 아니면 "더 보기"로 뒤에 붙은 줄만
	virtual void RebuildRows(bool bInFirstPage);
	void SetLoading(bool bInLoading);
	void SetFailed(bool bInFailed);
	void UpdateRefreshButtons(bool bInForce);
	bool IsMyBoard() const;

	//목록 줄을 넣는 칸(VerticalBox·ScrollBox)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Rows;

	//내 줄
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWarriorRankingRowWidget> Row_Mine;

	//기록이 없을 때 목록 자리 문구
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Empty;

	//불러오는 중 표시
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Loading;

	//실패 표시(안에 Text_Error·Button_Retry). 목록·내 줄 자리를 대신한다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Error;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Error;

	//목록·내 줄을 감싸는 칸(실패·불러오는 중이면 숨김). 없으면 Panel_Rows·Row_Mine을 따로 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Widget_Content;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Refresh;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Refresh;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Retry;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Retry;

	//"더 보기". 더 없으면 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_More;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking")
	TSubclassOf<UWarriorRankingRowWidget> RowClass;

	//가져오기 성공 뒤 새로 고침을 막는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking", meta = (ClampMin = "0.0"))
	float RefreshCooldownAfterSuccess = 10.f;

	//가져오기 실패 뒤 다시 불러오기를 막는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking", meta = (ClampMin = "0.0"))
	float RefreshCooldownAfterFailure = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText ErrorText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText RefreshText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText RetryText;

	//{0} 버튼 글자, {1} 남은 초
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText CooldownFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText LoadingRefreshText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText EmptyText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText NoRecordText;

	//막힌 버튼 색(채도 0·어둡게). 풀리면 흰색(원래 스타일)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Look")
	FLinearColor InactiveButtonColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.f);

	//"더 보기"를 쓰는 화면인가(스테이지 순위 카드는 위 몇 명만이라 false)
	bool bAllowMore = true;

	FWarriorRankingBoardKey ViewKey;
	int32 ViewLimit = 20;
	bool bHasView = false;
	bool bLoading = false;
	bool bFailed = false;
	FWarriorRefreshCooldown Cooldown;
	int32 LastShownRemaining = -1;

	//다음 묶음(스크롤 자동 받기)이 실패하면 이 시각까지 다시 받지 않는다(FPlatformTime::Seconds)
	double AppendBlockedUntilSeconds = 0.0;
};

/** 메인메뉴 랭킹 팝업: [전체 총합][지역 총합], 전체·주간 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorRankingPopupWidget : public UWarriorRankingViewWidget
{
	GENERATED_BODY()

public:
	UWarriorRankingPopupWidget(const FObjectInitializer& ObjectInitializer);

	//팝업을 열 때마다(전체 총합·전체 기간부터)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ranking")
	void Open();

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//탭·기간이 정해질 때. 선택 테두리는 BP에서
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Ranking", meta = (DisplayName = "On Tab Set"))
	void BP_OnTabSet(bool bInTotal, EWarriorRankingPeriod InPeriod);

	UFUNCTION()
	void HandleTotalTabClicked();

	UFUNCTION()
	void HandleRegionTabClicked();

	UFUNCTION()
	void HandleAllClicked();

	UFUNCTION()
	void HandleWeeklyClicked();

	UFUNCTION()
	void HandleRegionSelected(FString InSelectedItem, ESelectInfo::Type InSelectionType);

	UFUNCTION()
	void HandleStageListUpdated();

	void FillRegions();
	void RequestCurrent();

	virtual void RebuildRows(bool bInFirstPage) override;

	//ListView 스크롤. 끝에 가까우면 다음 묶음(자동 받기)
	void HandleListScrolled(float InItemOffset, float InDistanceRemaining);
	void TryLoadMore();

	//순위 목록(ListView, 항목 줄 = RowClass와 같은 WBP_RankingRow). 받은 줄은 모두 메모리에 남고, 줄 위젯은 보이는 만큼만 만든다.
	//없으면 공통 보기의 Panel_Rows(ScrollBox)를 쓴다(자동 받기 없음)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ListView_Rows;

	//남은 스크롤이 이 비율(0~1, 0 = 맨 아래) 아래로 내려가면 다음 묶음을 받는다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Scroll", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LoadMoreThreshold = 0.15f;

	//다음 묶음 요청 사이 최소 간격(초). 빠른 스크롤에도 이보다 자주 보내지 않는다(한 번에 한 요청은 서브시스템이 막음)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Scroll", meta = (ClampMin = "0.0"))
	float LoadMoreMinInterval = 0.5f;

	//한 보드에서 메모리에 둘 최대 줄 수. 넘으면 더 받지 않는다(안전 상한)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Scroll", meta = (ClampMin = "1"))
	int32 MaxLoadedRows = 500;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabTotal;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabRegion;

	//지역 고르기(지역 총합 탭에서만 보임). 항목은 지역 화면 이름
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ComboBox_Region;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_PeriodAll;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_PeriodWeekly;

	//점수 설명 한 줄
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Meta;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText TotalTitle;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText RegionTitle;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText TotalMeta;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText RegionMeta;

	//한 묶음 줄 수(명세 limit, 1~50)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking", meta = (ClampMin = "1", ClampMax = "50"))
	int32 PageSize = 20;

private:
	bool bTotalTab = true;
	EWarriorRankingPeriod Period = EWarriorRankingPeriod::All;
	FName SelectedRegionId;

	//콤보 항목 순서 = 지역 order 순
	TArray<FName> RegionIds;

	double LastLoadMoreSeconds = 0.0;
};

/** 스테이지 순위 카드: 스테이지 선택 팝업 위 한 겹. 위 TopCount명 + 내 순위(전체 기간) */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageRankingCardWidget : public UWarriorRankingViewWidget
{
	GENERATED_BODY()

public:
	UWarriorStageRankingCardWidget(const FObjectInitializer& ObjectInitializer);

	//그 스테이지 순위를 띄운다. InStageName은 제목에 쓴다
	void ShowStage(FName InStageId, const FText& InStageName, int32 InStageOrder);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	//{0} 스테이지 순서, {1} 스테이지 이름
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking|Text")
	FText TitleFormat;

	//보여 줄 위 몇 명(명세 limit)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Ranking", meta = (ClampMin = "1", ClampMax = "50"))
	int32 TopCount = 5;
};
