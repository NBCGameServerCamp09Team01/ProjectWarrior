// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "ProjectWarrior/DataAssets/DataAsset_InputConfig.h"
#include "WarriorInputComponent.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:
	template<class UserObject, typename CallbackFunc>
	void BindNativeInputAction(const UDataAsset_InputConfig* _InputConfig, const FGameplayTag& _InputTag, ETriggerEvent _TriggerEvent, UserObject* _ContextObject, CallbackFunc _Func);

	template<class UserObject, typename CallbackFunc>
	void BindAbilityInputAction(const UDataAsset_InputConfig* InInputConfig, UserObject* ContextObject, CallbackFunc InputPressedFunc, CallbackFunc InputRelasedFunc);
};

template<class UserObject, typename CallbackFunc>
inline void UWarriorInputComponent::BindNativeInputAction(const UDataAsset_InputConfig* _InputConfig, const FGameplayTag& _InputTag, ETriggerEvent _TriggerEvent, UserObject* _ContextObject, CallbackFunc _Func)
{
	checkf(_InputConfig, TEXT("Input config data asset is null, can not proceed with binding"));

	if (UInputAction* FoundAction = _InputConfig->FindNativeInputActionByTag(_InputTag))
	{
		BindAction(FoundAction, _TriggerEvent, _ContextObject, _Func);
	}
}

template<class UserObject, typename CallbackFunc>
inline void UWarriorInputComponent::BindAbilityInputAction(const UDataAsset_InputConfig* InInputConfig, UserObject* ContextObject, CallbackFunc InputPressedFunc, CallbackFunc InputRelasedFunc)
{
	checkf(InInputConfig, TEXT("Input config data asset is null,can not proceed with binding"));

	for (const FWarriorInputActionConfig& AbilityInputActionConfig : InInputConfig->AbilityInputActions)
	{
		if (!AbilityInputActionConfig.IsValid()) continue;

		BindAction(AbilityInputActionConfig.InputAction, ETriggerEvent::Started, ContextObject, InputPressedFunc, AbilityInputActionConfig.InputTag);
		BindAction(AbilityInputActionConfig.InputAction, ETriggerEvent::Completed, ContextObject, InputRelasedFunc, AbilityInputActionConfig.InputTag);
	}
}
