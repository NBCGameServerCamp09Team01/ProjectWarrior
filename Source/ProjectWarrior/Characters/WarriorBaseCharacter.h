// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "AbilitySystemInterface.h"
#include "ProjectWarrior/Interfaces/PawnCombatInterface.h"
#include "ProjectWarrior/Interfaces/PawnUIInterface.h"
#include "WarriorBaseCharacter.generated.h"

class UWarriorAbilitySystemComponent;
class UWarriorAttributeSet;
class UDataAsset_StartUpDataBase;
class UMotionWarpingComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorCharacterDied, AWarriorBaseCharacter*, DeadCharacter);

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorBaseCharacter : public AALSCharacter, public IAbilitySystemInterface, public IPawnCombatInterface, public IPawnUIInterface
{
	GENERATED_BODY()
	
public:
	AWarriorBaseCharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin AbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End AbilitySystemInterface Interface

	//~ Begin PawnCombatInterface Interface.
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface Interface

	//~ Begin PawnUIInterface Interface.
	virtual UPawnUIComponent* GetPawnUIComponent() const override;
	//~ End PawnUIInterface Interface

protected:
	//~ Begin APawn Interface.
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	UWarriorAbilitySystemComponent* WarriorAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	UWarriorAttributeSet* WarriorAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MotionWarping")
	UMotionWarpingComponent* MotionWarpingComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterData")
	TSoftObjectPtr<UDataAsset_StartUpDataBase> CharacterStartUpData;

public:
	UPROPERTY(BlueprintAssignable)
	FOnWarriorCharacterDied OnCharacterDied;

public:
	FORCEINLINE UWarriorAbilitySystemComponent* GetWarriorAbilitySystemComponent() const { return WarriorAbilitySystemComponent; }
	FORCEINLINE UWarriorAttributeSet* GetWarriorAttributeSet() const { return WarriorAttributeSet; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnCharacterDiedEvent();

	virtual void OnCharacterDiedEvent_Implementation();
};
