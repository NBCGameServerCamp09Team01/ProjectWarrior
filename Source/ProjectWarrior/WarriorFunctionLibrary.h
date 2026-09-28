// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "WarriorFunctionLibrary.generated.h"

class UWarriorAbilitySystemComponent;
class UPawnCombatComponent;
/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    static UWarriorAbilitySystemComponent* NativeGetWarriorASCFromActor(AActor* InActor);

    UFUNCTION(BlueprintCallable, Category = "Warrior|FunctionLibrary")
    static void AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd);

    UFUNCTION(BlueprintCallable, Category = "Warrior|FunctionLibrary")
    static void RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove);

    static bool NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck);

    UFUNCTION(BlueprintCallable, Category = "Warrior|FunctionLibrary", meta = (DisplayName = "Does Actor Have Tag", ExpandEnumAsExecs = "OutConfirmType"))
    static void BP_DoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck, EWarriorConfirmType& OutConfirmType);

    static bool NativeDoesActorHaveAnyTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck);

    UFUNCTION(BlueprintCallable, Category = "Warrior|FunctionLibrary", meta = (DisplayName = "Does Actor Have Any Tag", ExpandEnumAsExecs = "OutConfirmType"))
    static void BP_DoesActorHaveAnyTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck, EWarriorConfirmType& OutConfirmType);

    static bool NativeDoesActorHaveAllTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck);

    UFUNCTION(BlueprintCallable, Category = "Warrior|FunctionLibrary", meta = (DisplayName = "Does Actor Have All Tag", ExpandEnumAsExecs = "OutConfirmType"))
    static void BP_DoesActorHaveAllTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck, EWarriorConfirmType& OutConfirmType);

    static UPawnCombatComponent* NativeGetPawnCombatComponentFromActor(AActor* InActor);

    UFUNCTION(BlueprintCallable, Category = "Warrior|FunctionLibrary", meta = (DisplayName = "Get Pawn Combat Component From Actor", ExpandEnumAsExecs = "OutValidType"))
    static UPawnCombatComponent* BP_GetPawnCombatComponentFromActor(AActor* InActor, EWarriorValidType& OutValidType);

    UFUNCTION(BlueprintPure, Category = "Warrior|FunctionLibrary")
    static FGameplayTag ComputeHitReactDirectionTag(AActor* InAttacker, AActor* InVictim, float& OutAngleDifference);

    UFUNCTION(BlueprintPure, Category = "Warrior|FunctionLibrary")
    static bool IsValidBlock(AActor* InAttacker, AActor* InDefender);

    UFUNCTION(BlueprintPure, Category = "Warrior|FunctionLibrary")
    static bool IsTargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn);

};
