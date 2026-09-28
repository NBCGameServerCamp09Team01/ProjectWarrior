// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WarriorWidgetBase.generated.h"

class UPlayerUIComponent;
class UAIUIComponent;
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

public:
	UFUNCTION(BlueprintCallable)
	void InitAICreatedWidget(AActor* OwningAIActor);
};
