// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawnUIComponent.h"
#include "AIUIComponent.generated.h"

// 수식어 표시(체력바 위 줄). 이름 줄(예: "강인 · 강력")과 등급 색. 수식어가 없으면 빈 줄
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAffixDisplayChangedDelegate, const FText&, AffixLine, FLinearColor, TierColor);

/**
 *
 */
UCLASS()
class PROJECTWARRIOR_API UAIUIComponent : public UPawnUIComponent
{
	GENERATED_BODY()

public:
	// 수식어가 붙은 뒤 AWarriorAICharacter가 부른다. 값을 들고 있다가 늦게 묶은 위젯도 GetAffixLine으로 읽는다
	void SetAffixDisplay(const FText& InAffixLine, const FLinearColor& InTierColor);

	const FText& GetAffixLine() const { return AffixLine; }
	const FLinearColor& GetTierColor() const { return TierColor; }

	UPROPERTY(BlueprintAssignable)
	FOnAffixDisplayChangedDelegate OnAffixDisplayChanged;

private:
	FText AffixLine;
	FLinearColor TierColor = FLinearColor::White;
};
