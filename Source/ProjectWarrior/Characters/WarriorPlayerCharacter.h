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
class UInventoryWheelWidget;
class UPlayerInteractionComponent;
class UGameplayEffect;

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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	UPlayerInteractionComponent* PlayerInteractionComponent;
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

	UFUNCTION()
	void Input_InventoryWheelStarted(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void Input_InventoryWheelCompleted(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void Input_Interact(const FInputActionValue& InputActionValue);

#pragma endregion

private:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryWheelWidget> InventoryWheelWidgetClass;

	UPROPERTY()
	UInventoryWheelWidget* InventoryWheelWidget;

private:
	UPROPERTY()
	AActor* CurrentLockedActor;

public:
	FORCEINLINE UPlayerCombatComponent* GetPlayerCombatComponent() const { return PlayerCombatComponent; }

	void SetCurrentLockedActor(AActor* _Target) { CurrentLockedActor = _Target; }

	UFUNCTION(BlueprintCallable)
	AActor* GetCurrentLockedActor() const { return CurrentLockedActor; }

protected:
	// 계정 성장(메인메뉴에서 투자한 스탯)을 적용하는 GE. Infinite, Account.Stat.* SetByCaller 모디파이어
	UPROPERTY(EditDefaultsOnly, Category = "Account")
	TSubclassOf<UGameplayEffect> AccountStatEffect;

private:
	void ApplyAccountStatBonuses();
};