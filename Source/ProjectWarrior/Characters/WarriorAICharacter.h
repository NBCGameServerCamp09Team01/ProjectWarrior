// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorBaseCharacter.h"
#include "WarriorAICharacter.generated.h"

class UAICombatComponent;
class UAIUIComponent;
class UWidgetComponent;

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorAICharacter : public AWarriorBaseCharacter
{
	GENERATED_BODY()

public:
	AWarriorAICharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin PawnCombatInterface Interface.
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface Interface

	//~ Begin PawnUIInterface Interface.
	virtual UPawnUIComponent* GetPawnUIComponent() const override;
	virtual UAIUIComponent* GetAIUIComponent() const override;
	//~ End PawnUIInterface Interface

	// 사망 처리 시작(OnDeathAbilityStart)에서 호출됨. 사망 알림 후 AI 회전·이동을 멈춤
	virtual void OnCharacterDiedEvent_Implementation() override;

protected:
	virtual void BeginPlay() override;

	//~ Begin APawn Interface.
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	UAICombatComponent* AICombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UAIUIComponent* AIUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* AIHealthWidgetComponent;


private:
	void InitAIStartUpData();

public:
	FORCEINLINE UAICombatComponent* GetAICombatComponent() const { return AICombatComponent; }
};
