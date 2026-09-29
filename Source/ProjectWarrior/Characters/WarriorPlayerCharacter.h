// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "WarriorPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UDataAsset_InputConfig;
class UPlayerCombatComponent;
class UPlayerUIComponent;
class UPlayerInventoryComponent;

struct FInputActionValue;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorPlayerCharacter : public AWarriorBaseCharacter
{
	GENERATED_BODY()
	
public:
	AWarriorPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin PawnCombatInterface Interface.
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface Interface

	//~ Begin PawnUIInterface Interface.
	virtual UPawnUIComponent* GetPawnUIComponent() const override;
	virtual UPlayerUIComponent* GetPlayerUIComponent() const override;
	//~ End PawnUIInterface Interface

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UPlayerInventoryComponent* GetPlayerInventoryComponent() const;

	// 테스트용 콘솔 명령
	// 예: DebugAddItem /Game/MyGameContents/Items/DA_Potion_Small.DA_Potion_Small 3
	UFUNCTION(Exec)
	void DebugAddItem(const FString& InItemPath, int32 InCount = 1);

	UFUNCTION(Exec)
	void DebugUseItem(const FString& InItemPath);

	UFUNCTION(Exec)
	void DebugAddGold(int32 InAmount);

protected:
	//~ Begin APawn Interface.
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	void BeginPlay() override;

private:
#pragma region Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	UPlayerCombatComponent* PlayerCombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UPlayerUIComponent* PlayerUIComponent;

#pragma endregion

#pragma region Inputs
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UDataAsset_InputConfig* InputConfigData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	FVector2D LastMoveInputVector;

	float LastMoveInputTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float DodgeDoubleTapTimeout = 0.5f;

	UFUNCTION()
	void Input_Move(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void Input_MoveCompleted(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void Input_Look(const FInputActionValue& InputActionValue);
	
	UFUNCTION()
	void Input_SwitchTargetTriggered(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void Input_SwitchTargetCompleted(const FInputActionValue& InputActionValue);

	FVector2D SwitchDirection = FVector2D::ZeroVector;

	UFUNCTION()
	void Input_LeftButton(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void Input_AbilityInputPressed(FGameplayTag _InputTag);

	UFUNCTION()
	void Input_AbilityInputReleased(FGameplayTag _InputTag);

#pragma endregion

private:
	UPROPERTY()
	AActor* CurrentLockedActor;

public:
	FORCEINLINE UPlayerCombatComponent* GetPlayerCombatComponent() const { return PlayerCombatComponent; }

	void SetCurrentLockedActor(AActor* _Target) { CurrentLockedActor = _Target; }

	UFUNCTION(BlueprintCallable)
	AActor* GetCurrentLockedActor() const { return CurrentLockedActor; }
};