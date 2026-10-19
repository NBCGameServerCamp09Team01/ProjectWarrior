// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateColor.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageHUDWidget.generated.h"

class UTextBlock;
class AWarriorStageGameState;
class UPlayerInventoryComponent;

/**
 * 스테이지 진행 HUD의 베이스. 웨이브·상태·남은 시간·남은 적·보유 골드를 보여 준다.
 * 글자 채우기와 연출(알림, 골드 증가)은 이 클래스가 하고, WBP_StageHUD는 아래 이름의 위젯을 배치만 한다.
 * 값은 AWarriorStageGameState와 PlayerState의 UPlayerInventoryComponent에서 받는다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorStageHUDWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	//~ Begin GameState·인벤토리 알림
	UFUNCTION()
	void HandleStageStateChanged(EWarriorStageState InNewState, EWarriorStageState InOldState);

	UFUNCTION()
	void HandleWaveChanged(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave);

	UFUNCTION()
	void HandleEnemyCountChanged(int32 InAliveCount, int32 InTotalCount);

	UFUNCTION()
	void HandleGoldChanged(int32 InNewGold);
	//~ End GameState·인벤토리 알림

	//~ Begin WBP에 같은 이름으로 있어야 하는 위젯
	//"웨이브 2 / 5"
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Wave;

	//상태 문구 (준비, 전투, 쉬는 시간 등)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_State;

	//현재 상태의 남은 시간 "0:17". 타이머가 없는 상태(전투)에서는 숨긴다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Timer;

	//"남은 적 3 / 5". 전투 중에만 보인다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_EnemyCount;

	//보유 골드
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Gold;
	//~ End WBP에 같은 이름으로 있어야 하는 위젯

	//~ Begin 없어도 되는 위젯
	//보스 웨이브일 때만 보이는 표시
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_BossWave;

	//웨이브 시작·클리어 알림. 평소에는 숨긴다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Announce;

	//골드 증가 "+3". 연출이 끝나는 자리(골드 바로 아래)에 배치한다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_GoldDelta;
	//~ End 없어도 되는 위젯

	//~ Begin 문구 (WBP 클래스 기본값에서 수정)
	//상태별 문구. 없는 상태는 빈 글자
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	TMap<EWarriorStageState, FText> StateLabels;

	//{0} 현재 웨이브, {1} 전체 웨이브
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText WaveFormat;

	//{0} 남은 적, {1} 전체 적
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText EnemyCountFormat;

	//{0} 늘어난 골드
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText GoldDeltaFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText BossWaveLabel;

	//웨이브 시작 알림. {0} 웨이브 번호
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText WaveStartAnnounceFormat;

	//보스 웨이브 시작 알림
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText BossWaveAnnounceText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Text")
	FText WaveClearedAnnounceText;
	//~ End 문구

	//~ Begin 연출 설정
	//보스 웨이브일 때 Text_Wave의 색. 평소 색은 WBP에서 Text_Wave에 지정한 색을 쓴다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Effect")
	FLinearColor BossWaveColor = FLinearColor(1.f, 0.35f, 0.25f);

	//알림이 보이는 전체 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Effect", meta = (ClampMin = "0.1"))
	float AnnounceDuration = 2.f;

	//알림이 끝나기 전 투명해지는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Effect", meta = (ClampMin = "0.0"))
	float AnnounceFadeTime = 0.5f;

	//골드 증가 표시가 올라가며 사라지는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Effect", meta = (ClampMin = "0.1"))
	float GoldDeltaDuration = 0.8f;

	//골드 증가 표시가 배치한 자리보다 얼마나 아래에서 시작하는지(픽셀)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage|Effect", meta = (ClampMin = "0.0"))
	float GoldDeltaRiseDistance = 40.f;
	//~ End 연출 설정

private:
	//초기화 중과 결과 상태에서는 HUD 전체를 숨긴다
	static bool IsHUDShownInState(EWarriorStageState InState);

	void RefreshState(EWarriorStageState InState);
	void RefreshWave(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave);
	void RefreshEnemyCount(int32 InAliveCount, int32 InTotalCount);
	void RefreshGold(int32 InGold);

	//남은 시간을 읽어, 표시하는 초가 바뀌었을 때만 글자를 고친다
	void UpdateTimer();

	void ShowAnnounce(const FText& InText);
	void UpdateAnnounce(float InDeltaTime);
	void StopAnnounce();

	//연출 중에 또 들어오면 합산해서 처음부터 다시 보여 준다
	void PlayGoldDelta(int32 InDelta);
	void UpdateGoldDelta(float InDeltaTime);
	void StopGoldDelta();

	TWeakObjectPtr<AWarriorStageGameState> BoundGameState;
	TWeakObjectPtr<UPlayerInventoryComponent> BoundInventory;

	//WBP에서 Text_Wave에 지정한 색. 보스 웨이브가 끝나면 이 색으로 되돌린다
	FSlateColor NormalWaveColor;
	bool bNormalWaveColorCached = false;

	//지금 화면에 표시한 남은 초. INDEX_NONE이면 다음 UpdateTimer에서 무조건 다시 그린다
	int32 DisplayedTimerSeconds = INDEX_NONE;

	int32 LastGold = 0;

	bool bAnnounceActive = false;
	float AnnounceElapsed = 0.f;

	bool bGoldDeltaActive = false;
	float GoldDeltaElapsed = 0.f;
	int32 GoldDeltaAmount = 0;
};
