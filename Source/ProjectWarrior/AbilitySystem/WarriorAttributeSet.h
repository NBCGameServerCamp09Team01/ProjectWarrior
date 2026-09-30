// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "WarriorAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
 GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
 GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
 GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
 GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

class IPawnUIInterface;
class UPawnUIComponent;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
    UWarriorAttributeSet();

    virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

    virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

    UPROPERTY(BlueprintReadOnly, Category = "Health")
    FGameplayAttributeData CurrentHealth;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, CurrentHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Health")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Staimina")
    FGameplayAttributeData CurrentStamina;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, CurrentStamina)

    UPROPERTY(BlueprintReadOnly, Category = "Staimina")
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, MaxStamina)

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    FGameplayAttributeData DefensePower;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, DefensePower)

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    FGameplayAttributeData DamageTaken;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, DamageTaken)

    UPROPERTY(BlueprintReadOnly, Category = "Balance")
    FGameplayAttributeData MaxBalance;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, MaxBalance)

    UPROPERTY(BlueprintReadOnly, Category = "Balance")
    FGameplayAttributeData CurrentBalance;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, CurrentBalance)

    UPROPERTY(BlueprintReadOnly, Category = "Balance")
    FGameplayAttributeData BalanceDamageTaken;
    ATTRIBUTE_ACCESSORS(UWarriorAttributeSet, BalanceDamageTaken)

private:
    TWeakInterfacePtr<IPawnUIInterface> CachedPawnUIInterface;

    // PostGameplayEffectExecute 밖에서도 UI 컴포넌트를 얻기 위한 헬퍼 (checkf 대신 nullptr 허용)
    UPawnUIComponent* FindPawnUIComponent() const;
};
