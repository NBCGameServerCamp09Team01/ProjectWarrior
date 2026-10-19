// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "ProjectWarrior/Stats/WarriorStatTypes.h"
#include "ProjectWarrior/Account/WarriorAccountTypes.h"
#include "WarriorStageResultWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;

/**
 * 스테이지 결과 화면의 베이스.
 * 배치와 제목 채우기는 WBP_StageResult가 하고(On Result Set), 버튼은 아래 Request 함수만 호출한다.
 * 위젯은 AWarriorStagePlayerController가 끝 상태(StageCleared·StageFailed)에서 만든다.
 *
 * 통계·보상 줄은 이 클래스가 채운다. WBP에 아래 이름의 위젯을 두면 연결된다(없어도 된다).
 * - 도달 웨이브·플레이 시간: SetResult로 받은 결과(FWarriorStageResult). 플레이 시간은 실제 시간이다
 *   (통계의 PlayTimeSeconds는 월드 시간이라 히트 스톱 같은 시간 배율만큼 짧다).
 * - 처치 수·번 골드: 통계 기록(FWarriorStageRecord, UWarriorProfileStatsSubsystem::OnStageRecorded)
 * - 경험치·레벨 업: 계정 보상(FWarriorStageReward, UWarriorAccountSubsystem::OnStageRewarded).
 *   판이 끝나면 경험치를 받고, 경험치가 차서 레벨이 오를 때만 스탯 포인트가 생긴다. 그래서 레벨 변화와
 *   스탯 포인트는 LevelUpBox에 묶어 레벨이 올랐을 때만 보인다.
 * 기록과 보상은 위젯이 만들어진 직후 같은 프레임에 온다(결과 확정 → 상태 변경 → 기록 마감 → 보상 계산).
 * 그래서 만들어질 때 직접 구독하고, 이번 스테이지의 통계 기록 번호(RecordId)와 같은 것만 표시한다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorStageResultWidget(const FObjectInitializer& ObjectInitializer);

	//결과를 저장하고 도달 웨이브·플레이 시간 줄을 채운 뒤 BP_OnResultSet을 호출한다
	void SetResult(const FWarriorStageResult& InResult);

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	const FWarriorStageResult& GetResult() const { return Result; }

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//결과가 들어오면 WBP에서 제목 글자를 채운다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Stage", meta = (DisplayName = "On Result Set"))
	void BP_OnResultSet(const FWarriorStageResult& InResult);

	//"다시 하기" 버튼용. 소유 컨트롤러의 RestartStage를 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RequestRestart();

	//"메인메뉴" 버튼용. 소유 컨트롤러의 ReturnToMainMenu를 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RequestReturnToMainMenu();

	//~ Begin 통계·계정 알림
	//통계 보관소의 OnStageRecorded에 연결. 이번 스테이지의 기록이 아니면 무시한다
	UFUNCTION()
	void HandleStageRecorded(const FWarriorStageRecord& InStageRecord);

	//계정 서브시스템의 OnStageRewarded에 연결. 이번 스테이지의 보상이 아니면 무시한다
	UFUNCTION()
	void HandleStageRewarded(const FWarriorStageReward& InReward);
	//~ End 통계·계정 알림

	//~ Begin 없어도 되는 위젯 (WBP에 같은 이름으로 두면 연결된다)
	//"3 / 5"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ReachedWave;

	//"4:12"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PlayTime;

	//"27"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Kills;

	//"340"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_GoldEarned;

	//획득 경험치 "+100"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ExpGained;

	//보상 뒤 현재 레벨 안의 경험치 "50 / 200". 최대 레벨이면 "MAX"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ExpProgress;

	//Text_ExpProgress와 같은 값의 막대(0~1)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Exp;

	//레벨이 올랐을 때만 보이는 묶음(Text_LevelChange, Text_StatPointsGained를 담는다). 평소에는 접혀 있다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LevelUpBox;

	//"Lv. 1 → 3". 레벨이 오르지 않았으면 "Lv. 1"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LevelChange;

	//레벨 업으로 받은 스탯 포인트 "+4"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StatPointsGained;
	//~ End 없어도 되는 위젯

	//~ Begin 문구 (WBP 클래스 기본값에서 수정)
	//{0} 도달한 웨이브, {1} 전체 웨이브
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText ReachedWaveFormat;

	//{0} 얻은 경험치
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText ExpGainedFormat;

	//{0} 보상 전 레벨, {1} 보상 뒤 레벨. 레벨이 올랐을 때
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText LevelUpFormat;

	//{0} 레벨. 레벨이 오르지 않았을 때
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText LevelFormat;

	//{0} 레벨 업으로 받은 스탯 포인트
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText StatPointsGainedFormat;

	//{0} 현재 레벨 안의 경험치, {1} 다음 레벨까지 필요한 경험치
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText ExpProgressFormat;

	//최대 레벨일 때 경험치 진행도 대신 표시
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText MaxLevelText;

	//값을 받지 못한 칸(통계·계정 서브시스템이 없거나 스테이지가 아닌 레벨)에 표시
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText EmptyValueText;
	//~ End 문구

private:
	void ApplyStageRecord(const FWarriorStageRecord& InStageRecord);
	void ApplyStageReward(const FWarriorStageReward& InReward);

	UPROPERTY(Transient)
	FWarriorStageResult Result;

	//이번 스테이지의 통계 기록 번호(FWarriorStageRecord::RecordId). 이 번호의 기록·보상만 표시한다.
	//결과의 RunId(GameMode가 발급)와는 다른 번호다(리팩토링 R2).
	FGuid ExpectedRecordId;
};
