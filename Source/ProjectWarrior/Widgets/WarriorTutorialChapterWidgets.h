// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorTutorialChapterWidgets.generated.h"

class UButton;
class UDataAsset_TutorialSteps;
class UPanelWidget;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWarriorTutorialChapterClicked, int32 /*ChapterIndex*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorTutorialChapterPopupClosed);

/** 장 고르기 팝업의 한 줄: "3장 방어 · 7단계". 누르면 그 장부터 튜토리얼을 연다. */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorTutorialChapterEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorTutorialChapterEntryWidget(const FObjectInitializer& ObjectInitializer);

	//InChapterIndex는 0부터
	void SetChapter(int32 InChapterIndex, const FText& InTitle, int32 InStepCount);

	//팝업이 처음 포커스를 줄 버튼
	UButton* GetEntryButton() const { return Button_Entry; }

	FOnWarriorTutorialChapterClicked OnChapterClicked;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleEntryClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Entry;

	//"3장 방어"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ChapterTitle;

	//"7단계"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ChapterInfo;

	//{0} 장 번호, {1} 장 제목
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial|Text")
	FText ChapterTitleFormat;

	//{0} 단계 수
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial|Text")
	FText ChapterInfoFormat;

private:
	int32 ChapterIndex = INDEX_NONE;
};

/**
 * 튜토리얼 장 고르기 팝업(계획서 2장 I). 메인메뉴 위에 뜬다(스테이지 선택·랭킹 팝업과 같은 틀).
 * 프론트 컨트롤러가 OpenTutorialSelect에서 띄우고 Open으로 장 목록(UDataAsset_TutorialSteps)을 넘긴다.
 * 장을 고르면 프론트 컨트롤러의 EnterTutorial로 넘긴다. 완료 기록이 없어서(D2) 장마다 진행 표시는 없다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorTutorialChapterPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorTutorialChapterPopupWidget(const FObjectInitializer& ObjectInitializer);

	//장 목록을 다시 그린다(팝업을 열 때마다). InSteps가 없으면 목록을 비운다
	void Open(const UDataAsset_TutorialSteps* InSteps);

	//팝업을 닫고 OnClosed로 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Tutorial")
	void Close();

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Tutorial")
	FOnWarriorTutorialChapterPopupClosed OnClosed;

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	UFUNCTION()
	void HandleCloseClicked();

	void HandleChapterClicked(int32 InChapterIndex);

	//장 줄을 넣는 칸(VerticalBox 또는 ScrollBox)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Chapters;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	//장 한 줄(WBP_TutorialChapterEntry)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial")
	TSubclassOf<UWarriorTutorialChapterEntryWidget> ChapterEntryClass;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWarriorTutorialChapterEntryWidget>> ChapterEntries;
};
