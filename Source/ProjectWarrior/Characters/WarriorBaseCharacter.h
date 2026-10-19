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
class UNiagaraSystem;
class UNiagaraComponent;

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

	// 이름별 이동 속도 배율(신속 수식어, 냉기 둔화 등). 1이면 그 이름을 지운다.
	// 모든 배율을 곱해 UWarriorCharacterMovementComponent의 최종 최고 속도에 반영한다(MaxWalkSpeed는 ALS·락온 등이 정한 값 그대로)
	void SetMovementSpeedScale(FName InKey, float InScale);

	// InDuration초 동안만 배율을 건다. 같은 이름으로 다시 부르면 시간을 새로 고친다(세기는 새 값)
	void ApplyTimedMovementSpeedScale(FName InKey, float InScale, float InDuration);

	float GetMovementSpeedScale() const;

	// 이름별로 InDuration초 동안 보이는 메시에 상태 이펙트를 붙인다(수식어 화상·둔화가 걸린 동안). 같은 이름으로 다시 부르면 시간만 새로 고친다(겹쳐 생기지 않음)
	void ShowTimedStatusEffect(FName InKey, UNiagaraSystem* InSystem, const FLinearColor& InColor, float InDuration);

private:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNiagaraComponent>> StatusEffectComponents;
	TMap<FName, FTimerHandle> StatusEffectTimers;

	void RefreshMovementSpeedScale();

	TMap<FName, float> MovementSpeedScales;
	TMap<FName, FTimerHandle> MovementSpeedScaleTimers;
};
