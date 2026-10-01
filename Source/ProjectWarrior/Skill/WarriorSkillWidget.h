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
 * - SkillList와 EntryWidgetClass가 있으면 스킬마다 줄 위젯을 만들어 채운다. BP 그래프 없이 목록이 나온다.
 * - 버튼은 RequestUnlock / RequestBack만 부른다. 해금 판단과 포인트 차감은 UWarriorSkillLibrary와 계정이 한다.
 * - 화면 배치는 WBP에서 한다. 지금은 목록 틀, 디자인 단계에서 트리로 바꿔도 넘기는 값은 같다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorSkillWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorSkillWidget(const FObjectInitializer& ObjectInitializer);

	/** 스킬 하나 해금 요청. 성공하면 계정 변경 알림으로 화면이 갱신된다. 실패하면 false */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Skill")
	bool RequestUnlock(FGameplayTag SkillTag);

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

	/** 해금 요청이 거절됐을 때 (상태가 바뀌었거나 계정에 등록되지 않은 스킬). 안내 문구를 띄울 때 쓴다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Skill")
	void BP_OnUnlockFailed(FGameplayTag SkillTag);

	UFUNCTION()
	void HandleAccountChanged(const FWarriorAccountData& InAccountData);

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
	//~ End 없어도 되는 위젯

	//{0} 계정 레벨
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText AccountLevelFormat;

	//{0} 남은 스탯 포인트
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText StatPointsFormat;

private:
	UPROPERTY(Transient)
	TArray<FWarriorSkillView> SkillViews;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWarriorSkillEntryWidget>> EntryWidgets;
};
