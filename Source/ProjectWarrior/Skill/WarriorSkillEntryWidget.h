#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorSkillTypes.h"
#include "WarriorSkillEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UWarriorSkillWidget;

/**
 * 스킬 화면의 한 줄 (SK).
 * - UWarriorSkillWidget이 스킬마다 하나씩 만들고 SetView로 값을 넘긴다.
 * - 위젯은 모두 없어도 된다. WBP_SkillEntry에 같은 이름으로 두면 연결된다.
 * - 해금 버튼은 소유 화면의 RequestUnlock만 부른다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorSkillEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorSkillEntryWidget(const FObjectInitializer& ObjectInitializer);

	/** 스킬 한 줄을 표시한다. InOwner는 해금 요청을 받을 스킬 화면 */
	void SetView(const FWarriorSkillView& InView, UWarriorSkillWidget* InOwner);

	UFUNCTION(BlueprintPure, Category = "Warrior|Skill")
	const FWarriorSkillView& GetView() const { return View; }

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** SetView 뒤에 호출. 디자인 단계에서 아이콘·진행 바 등을 BP에서 더 그릴 때 쓴다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Skill")
	void BP_OnViewSet(const FWarriorSkillView& InView);

	UFUNCTION()
	void HandleUnlockButtonClicked();

	//~ Begin 없어도 되는 위젯 (WBP에 같은 이름으로 두면 연결된다)
	//"강공격"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	//"선행: HeavyAttack · 누적 처치 30 이상 (42 / 30)"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Requirement;

	//"해금 가능"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_State;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Unlock;

	//"해금" / "완료"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Unlock;
	//~ End 없어도 되는 위젯

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText UnlockButtonText;

	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Skill|Text")
	FText UnlockedButtonText;

private:
	UPROPERTY(Transient)
	FWarriorSkillView View;

	TWeakObjectPtr<UWarriorSkillWidget> OwnerSkillWidget;
};
