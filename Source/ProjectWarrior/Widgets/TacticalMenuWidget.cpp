// Fill out your copyright notice in the Description page of Project Settings.


#include "TacticalMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "TacticalMenu"

UTextBlock* UTacticalMenuWidget::MakeText(const FText& InText, const FLinearColor& InColor, int32 InFontSize)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(InText);
	Text->SetColorAndOpacity(FSlateColor(InColor));

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = InFontSize;
	Text->SetFont(Font);

	return Text;
}

UWidget* UTacticalMenuWidget::MakeRow(const FText& InName, const FText& InDetail, bool bInSelected, bool bInEnabled)
{
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Row->SetPadding(RowPadding);

	// 선택된 줄만 파란 띠, 나머지는 배경 없음
	Row->SetBrushColor(bInSelected ? SelectedRowColor : FLinearColor::Transparent);

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(Line);

	UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(MakeText(InName, bInEnabled ? TextColor : DisabledColor, FontSize));
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameSlot->SetVerticalAlignment(VAlign_Center);

	if (!InDetail.IsEmpty())
	{
		UHorizontalBoxSlot* DetailSlot = Line->AddChildToHorizontalBox(MakeText(InDetail, bInEnabled ? DetailColor : DisabledColor, DetailFontSize));
		DetailSlot->SetVerticalAlignment(VAlign_Center);
		DetailSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
	}

	return Row;
}

void UTacticalMenuWidget::SetEntries(const FText& InHeader, const TArray<FTacticalMenuEntry>& InEntries, int32 InSelectedIndex)
{
	if (HeaderText)
	{
		HeaderText->SetText(InHeader);
	}

	if (!CommandBox)
	{
		return;
	}

	CommandBox->ClearChildren();

	for (int32 Index = 0; Index < InEntries.Num(); ++Index)
	{
		const FTacticalMenuEntry& Entry = InEntries[Index];
		UVerticalBoxSlot* RowSlot = CommandBox->AddChildToVerticalBox(MakeRow(Entry.Name, Entry.Detail, Index == InSelectedIndex, Entry.bEnabled));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(0.0f, 2.0f));
	}

	BP_OnMenuUpdated();
}

void UTacticalMenuWidget::SetStocks(const TArray<FText>& InStockLines, int32 InMaxStocks)
{
	if (!StockBox)
	{
		return;
	}

	StockBox->ClearChildren();

	for (int32 Index = 0; Index < InMaxStocks; ++Index)
	{
		const bool bFilled = InStockLines.IsValidIndex(Index);
		const FText Line = FText::Format(LOCTEXT("StockLine", "{0}. {1}"), FText::AsNumber(Index + 1),
			bFilled ? InStockLines[Index] : LOCTEXT("EmptyStock", "-"));

		UVerticalBoxSlot* RowSlot = StockBox->AddChildToVerticalBox(MakeText(Line, bFilled ? TextColor : EmptyStockColor, DetailFontSize));
		RowSlot->SetPadding(FMargin(16.0f, 4.0f));
	}

	BP_OnMenuUpdated();
}

void UTacticalMenuWidget::SetHint(const FText& InHint)
{
	if (HintText)
	{
		HintText->SetText(InHint);
	}
}

void UTacticalMenuWidget::SetTarget(AActor* InTarget)
{
	TargetActor = InTarget;

	if (TargetMarker)
	{
		TargetMarker->SetVisibility(InTarget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTacticalMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AActor* Target = TargetActor.Get();
	UCanvasPanelSlot* MarkerSlot = TargetMarker ? Cast<UCanvasPanelSlot>(TargetMarker->Slot) : nullptr;

	if (!Target || !MarkerSlot)
	{
		return;
	}

	// 캡슐 꼭대기 위에 표시
	const UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector MarkerWorldLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + TargetMarkerHeightOffset);

	FVector2D ScreenPosition;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), MarkerWorldLocation, ScreenPosition, false))
	{
		MarkerSlot->SetPosition(ScreenPosition);
	}
}

#undef LOCTEXT_NAMESPACE
