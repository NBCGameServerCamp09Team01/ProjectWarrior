// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorWidgetBase.h"
#include "Components/TextBlock.h"
#include "ProjectWarrior/Interfaces/PawnUIInterface.h"
#include "ProjectWarrior/Components/UI/PlayerUIComponent.h"
#include "ProjectWarrior/Components/UI/AIUIComponent.h"

void UWarriorWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(GetOwningPlayerPawn()))
	{
		if (UPlayerUIComponent* PlayerUIComponent = PawnUIInterface->GetPlayerUIComponent())
		{
			BP_OnOwningPlayerUIComponentInitialized(PlayerUIComponent);
		}
	}
}

void UWarriorWidgetBase::InitAICreatedWidget(AActor* OwningAIActor)
{
	if (IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(OwningAIActor))
	{
		UAIUIComponent* AIUIComponent = PawnUIInterface->GetAIUIComponent();

		checkf(AIUIComponent, TEXT("Failed to extrac an AIUIComponent from %s"), *OwningAIActor->GetActorNameOrLabel());

		// 수식어 줄: 수식어는 보통 이 뒤(스폰 직후·시작 데이터 로드 뒤)에 오므로 이벤트로 받고, 이미 온 값도 바로 넣는다
		if (Text_AffixLine)
		{
			AIUIComponent->OnAffixDisplayChanged.AddUniqueDynamic(this, &ThisClass::HandleAffixDisplayChanged);
			HandleAffixDisplayChanged(AIUIComponent->GetAffixLine(), AIUIComponent->GetTierColor());
		}

		BP_OnOwningAIUIComponentInitialized(AIUIComponent);
	}
}

void UWarriorWidgetBase::HandleAffixDisplayChanged(const FText& InAffixLine, FLinearColor InTierColor)
{
	if (!Text_AffixLine)
	{
		return;
	}

	Text_AffixLine->SetText(InAffixLine);
	Text_AffixLine->SetColorAndOpacity(FSlateColor(InTierColor));
	Text_AffixLine->SetVisibility(InAffixLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
