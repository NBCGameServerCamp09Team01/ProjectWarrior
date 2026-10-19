#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Account/WarriorAccountTypes.h"
#include "WarriorSkillTypes.h"
#include "WarriorSkillWidget.generated.h"

class UButton;
class UDataAsset_SkillTree;
class UPanelWidget;
class UTextBlock;
class UWarriorSkillEntryWidget;

/**
 * 스킬 화면의 베이스 (SK).
 * - 열릴 때와 계정 값이 바뀔 때(해금, 레벨 업 등) 스킬 목록을 다시 평가해 BP_OnSkillsRefreshed로 넘긴다.
 * - 로그인 상태면 열릴 때 서버 진행도(K2)를 받아 그 값으로 그린다(S4). 받기 전·로그인 전에는 DA + 로컬 통계로 그린다.
 * - SkillList와 EntryWidgetClass가 있으면 스킬마다 줄 위젯을 만들어 채운다. BP 그래프 없이 목록이 나온다.
 * - 버튼은 RequestUnlock / RequestBack만 부른다. 해금 판단과 포인트 차감은 서버가 한다(UWarriorSkillUnlockSubsystem).
 * - 해금 결과 문구는 Text_Message(있으면)와 BP_OnUnlockFinished로 알린다. 응답을 기다리는 동안 해금 버튼은 꺼진다(IsUnlocking).
 * - 화면 배치는 WBP에서 한다. 지금은 목록 틀, 디자인 단계에서 트리로 바꿔도 넘기는 값은 같다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorSkillWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorSkillWidget(const FObjectInitializer& ObjectInitializer);

	/** 스킬 하나 해금 요청(서버). 요청을 보냈으면 true(결과는 BP_OnUnlockFinished). 이미 기다리는 중이면 false */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill")
	bool RequestUnlock(FGameplayTag SkillTag);

	/** 해금 요청을 보내고 응답을 기다리는 중 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	bool IsUnlocking() const;

	/** 메인메뉴로 돌아간다 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill")
	void RequestBack();

	/** 다시 평가해서 화면을 갱신한다 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill")
	void Refresh();

	/** 마지막으로 평가한 스킬 목록 (DA 순서) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	const TArray<FWarriorSkillView>& GetSkillViews() const { return SkillViews; }

	/** 마지막 평가 결과에서 스킬 하나를 찾는다. 없으면 false */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	bool FindSkillView(FGameplayTag SkillTag, FWarriorSkillView& OutView) const;

	//~ 틀 단계 표시용 문구 (디자인 단계에서 WBP 쪽으로 옮겨도 된다)
	/** 상태 문구: 해금됨 / 해금 가능 / 포인트 부족 / 잠김 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	static FText GetStateText(EWarriorSkillState State);

	/** 조건 한 줄: "누적 처치 30 이상 (42 / 30)". 이하 조건은 "(312 / 300 이하)", 기록 없으면 "(기록 없음)" */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	static FText GetRequirementLine(const FWarriorSkillRequirementProgress& Progress);

	/** 스킬 하나의 조건 요약: 선행 스킬 + 조건을 " · "로 이은 한 줄. 조건이 없으면 "조건 없음" */
	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	static FText GetRequirementSummary(const FWarriorSkillView& View);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 화면에 보여 줄 스킬 목록 (WBP_Skill에서 DA_SkillTree 지정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Skill")
	TObjectPtr<UDataAsset_SkillTree> SkillTree;

	/** 화면이 열릴 때와 값이 바뀔 때 호출. BP에서 레벨·포인트·스킬 줄을 다시 그린다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Skill")
	void BP_OnSkillsRefreshed(const TArray<FWarriorSkillView>& Views, int32 AccountLevel, int32 StatPoints);

	/** 해금 요청이 거절됐을 때 (조건 미충족·이미 해금·로그인 필요 등). 안내 문구를 띄울 때 쓴다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Skill")
	void BP_OnUnlockFailed(FGameplayTag SkillTag);

	/** 해금 요청이 끝났을 때(성공·실패). 실패면 Message에 화면 문구, 성공이면 빈 문구 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Skill")
	void BP_OnUnlockFinished(FGameplayTag SkillTag, bool bSuccess, const FText& Message);

	UFUNCTION()
	void HandleAccountChanged(const FWarriorAccountData& InAccountData);

	UFUNCTION()
	void HandleUnlockCompleted(bool bSuccess, FGameplayTag SkillTag, const FString& ErrorCode, const FText& Message);

	UFUNCTION()
	void HandleSkillsReceived(bool bSuccess);

	UFUNCTION()
	void HandleBackButtonClicked();

	/** 계정 레벨·포인트 문구를 갱신한다 */
	void UpdateSummaryTexts(int32 AccountLevel, int32 StatPoints);

	/** SkillList에 스킬마다 줄 위젯을 채운다. 만든 줄은 다시 쓰고 남는 줄은 숨긴다 */
	void UpdateEntries();

	/** 스킬 한 줄 위젯 (WBP_SkillEntry). 비우면 줄을 만들지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill")
	TSubclassOf<UWarriorSkillEntryWidget> EntryWidgetClass;

	//~ Begin 없어도 되는 위젯 (WBP에 같은 이름으로 두면 연결된다)
	/** 줄 위젯을 넣을 패널 (VerticalBox 등) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SkillList;

	//"계정 레벨 3"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AccountLevel;

	//"스탯 포인트 2"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StatPoints;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Back;

	//해금 결과 문구("해금했습니다." / 실패 문구)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Message;
	//~ End 없어도 되는 위젯

	//{0} 계정 레벨
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText AccountLevelFormat;

	//{0} 남은 스탯 포인트
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText StatPointsFormat;

	//해금 성공 문구
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText UnlockSucceededText;

private:
	UPROPERTY(Transient)
	TArray<FWarriorSkillView> SkillViews;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWarriorSkillEntryWidget>> EntryWidgets;
};
