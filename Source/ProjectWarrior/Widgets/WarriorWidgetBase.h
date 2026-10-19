// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorWidgetBase.generated.h"

class UPlayerUIComponent;
class UAIUIComponent;
class UTextBlock;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Owning Player UI Component Initialized"))
	void BP_OnOwningPlayerUIComponentInitialized(UPlayerUIComponent* OwningPlayerUIComponent);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Owning AI UI Component Initialized"))
	void BP_OnOwningAIUIComponentInitialized(UAIUIComponent* OwningAIUIComponent);

	// AI 체력바(WBP_AIStatusBar)의 수식어 줄. 있으면 UAIUIComponent::OnAffixDisplayChanged를 받아 글자·등급 색을 넣고, 수식어가 없으면 접는다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AffixLine;

public:
	UFUNCTION(BlueprintCallable)
	void InitAICreatedWidget(AActor* OwningAIActor);

private:
	UFUNCTION()
	void HandleAffixDisplayChanged(const FText& InAffixLine, FLinearColor InTierColor);
};
