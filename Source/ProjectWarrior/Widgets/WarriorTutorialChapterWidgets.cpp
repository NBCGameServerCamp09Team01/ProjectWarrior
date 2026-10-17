// Fill out your copyright notice in the Description page of Project Settings.

#include "WarriorTutorialChapterWidgets.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Controllers/WarriorFrontPlayerController.h"
#include "ProjectWarrior/Tutorial/DataAsset_TutorialSteps.h"

#define LOCTEXT_NAMESPACE "WarriorTutorial"

//~ Begin UWarriorTutorialChapterEntryWidget

UWarriorTutorialChapterEntryWidget::UWarriorTutorialChapterEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChapterTitleFormat = LOCTEXT("ChapterTitleFormat", "{0}장 {1}");
	ChapterInfoFormat = LOCTEXT("ChapterInfoFormat", "{0}단계");
}

void UWarriorTutorialChapterEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button_Entry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEntryClicked);
}

void UWarriorTutorialChapterEntryWidget::SetChapter(int32 InChapterIndex, const FText& InTitle, int32 InStepCount)
{
	ChapterIndex = InChapterIndex;

	if (Text_ChapterTitle)
	{
		Text_ChapterTitle->SetText(FText::Format(ChapterTitleFormat, FText::AsNumber(InChapterIndex + 1), InTitle));
	}
	if (Text_ChapterInfo)
	{
		Text_ChapterInfo->SetText(FText::Format(ChapterInfoFormat, FText::AsNumber(InStepCount)));
	}
}

void UWarriorTutorialChapterEntryWidget::HandleEntryClicked()
{
	OnChapterClicked.Broadcast(ChapterIndex);
}

//~ End UWarriorTutorialChapterEntryWidget

//~ Begin UWarriorTutorialChapterPopupWidget

UWarriorTutorialChapterPopupWidget::UWarriorTutorialChapterPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//프론트 컨트롤러가 띄울 때 이 위젯에 포커스를 준다. 받아서 첫 장으로 넘기고 ESC를 받는다
	SetIsFocusable(true);
}

void UWarriorTutorialChapterPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UWarriorTutorialChapterPopupWidget::Open(const UDataAsset_TutorialSteps* InSteps)
{
	for (UWarriorTutorialChapterEntryWidget* Entry : ChapterEntries)
	{
		if (Entry)
		{
			Entry->OnChapterClicked.RemoveAll(this);
		}
	}
	ChapterEntries.Reset();
	Box_Chapters->ClearChildren();

	if (!InSteps)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] Chapter popup opened without tutorial steps."));
		return;
	}
	if (!ChapterEntryClass)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Tutorial] %s: ChapterEntryClass is not set."), *GetName());
		return;
	}

	const TArray<FWarriorTutorialChapter>& Chapters = InSteps->GetChapters();
	for (int32 ChapterIndex = 0; ChapterIndex < Chapters.Num(); ++ChapterIndex)
	{
		UWarriorTutorialChapterEntryWidget* Entry = CreateWidget<UWarriorTutorialChapterEntryWidget>(this, ChapterEntryClass);
		if (!Entry)
		{
			continue;
		}
		Entry->SetChapter(ChapterIndex, Chapters[ChapterIndex].Title, Chapters[ChapterIndex].Steps.Num());
		Entry->OnChapterClicked.AddUObject(this, &ThisClass::HandleChapterClicked);
		Box_Chapters->AddChild(Entry);
		ChapterEntries.Add(Entry);
	}
}

void UWarriorTutorialChapterPopupWidget::Close()
{
	RemoveFromParent();
	OnClosed.Broadcast();
}

FReply UWarriorTutorialChapterPopupWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	if (!ChapterEntries.IsEmpty() && ChapterEntries[0] && ChapterEntries[0]->GetEntryButton())
	{
		return FReply::Handled().SetUserFocus(ChapterEntries[0]->GetEntryButton()->TakeWidget(), InFocusEvent.GetCause());
	}
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

FReply UWarriorTutorialChapterPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UWarriorTutorialChapterPopupWidget::HandleCloseClicked()
{
	Close();
}

void UWarriorTutorialChapterPopupWidget::HandleChapterClicked(int32 InChapterIndex)
{
	if (AWarriorFrontPlayerController* FrontPlayerController = GetOwningPlayer<AWarriorFrontPlayerController>())
	{
		FrontPlayerController->EnterTutorial(InChapterIndex);
		return;
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] %s: owning player is not AWarriorFrontPlayerController. Chapter %d is ignored."), *GetName(), InChapterIndex);
}

//~ End UWarriorTutorialChapterPopupWidget

#undef LOCTEXT_NAMESPACE
