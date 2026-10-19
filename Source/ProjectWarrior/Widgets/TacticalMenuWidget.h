// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TacticalMenuWidget.generated.h"

class UVerticalBox;
class UTextBlock;

// 목록 한 줄(기술 또는 대상)
USTRUCT(BlueprintType)
struct FTacticalMenuEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tactical")
	FText Name;

	// 오른쪽에 작게 붙는 글(비용, 거리 등)
	UPROPERTY(BlueprintReadOnly, Category = "Tactical")
	FText Detail;

	// 고를 수 없으면 흐리게
	UPROPERTY(BlueprintReadOnly, Category = "Tactical")
	bool bEnabled = true;
};

/**
 * 전술 모드 커맨드 메뉴. 전술 모드 어빌리티가 상태를 넣어 주면 줄을 다시 그린다.
 * 기술을 고르는 동안은 기술 목록, 대상을 고르는 동안은 같은 패널에 대상 목록을 보여 준다(제목만 바뀜).
 * WBP에 CommandBox·StockBox(VerticalBox)가 있어야 하고, HeaderText·HintText(TextBlock)·TargetMarker(캔버스 패널 자식)는 선택.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UTacticalMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEntries(const FText& InHeader, const TArray<FTacticalMenuEntry>& InEntries, int32 InSelectedIndex);

	// 스톡 줄(이미 넣은 행동)과 최대 칸 수. 빈 칸은 자리만 보여 줌
	void SetStocks(const TArray<FText>& InStockLines, int32 InMaxStocks);

	void SetHint(const FText& InHint);

	// 지금 고르는 대상 위에 표시를 띄움(nullptr이면 숨김)
	void SetTarget(AActor* InTarget);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 줄을 다시 그린 뒤 부름(WBP에서 추가 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Tactical", meta = (DisplayName = "On Menu Updated"))
	void BP_OnMenuUpdated();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> CommandBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> StockBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	// 캔버스 패널 자식이어야 위치를 옮길 수 있음
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TargetMarker;

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	int32 FontSize = 20;

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	int32 DetailFontSize = 16;

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	FMargin RowPadding = FMargin(16.0f, 8.0f);

	// 선택된 줄의 배경(파란 띠)
	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	FLinearColor SelectedRowColor = FLinearColor(0.07f, 0.38f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	FLinearColor TextColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	FLinearColor DetailColor = FLinearColor(0.6f, 0.8f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	FLinearColor DisabledColor = FLinearColor(0.4f, 0.42f, 0.48f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	FLinearColor EmptyStockColor = FLinearColor(0.35f, 0.38f, 0.45f, 1.0f);

	// 대상 표시를 캡슐 위로 얼마나 올릴지(cm)
	UPROPERTY(EditAnywhere, Category = "Tactical|Style")
	float TargetMarkerHeightOffset = 40.0f;

private:
	UTextBlock* MakeText(const FText& InText, const FLinearColor& InColor, int32 InFontSize);

	// 배경 띠 + 이름(왼쪽) + 상세(오른쪽)
	UWidget* MakeRow(const FText& InName, const FText& InDetail, bool bInSelected, bool bInEnabled);

	TWeakObjectPtr<AActor> TargetActor;
};
