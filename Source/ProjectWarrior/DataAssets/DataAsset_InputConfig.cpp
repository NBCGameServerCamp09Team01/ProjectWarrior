// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAsset_InputConfig.h"

UInputAction* UDataAsset_InputConfig::FindNativeInputActionByTag(const FGameplayTag& _InputTag) const
{
	for (const FWarriorInputActionConfig& InputActionConfig : NativeInputActions)
	{
		if (InputActionConfig.InputTag == _InputTag && InputActionConfig.InputAction)
		{
			return InputActionConfig.InputAction;
		}
	}

	return nullptr;
}
