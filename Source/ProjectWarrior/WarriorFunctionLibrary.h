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

    // 근접/투사체 공통 피격 판정. InDamageCauser가 있으면 막기 방향 판정에 공격자 대신 사용 (예: 화살)
    UFUNCTION(BlueprintPure, Category = "Warrior|FunctionLibrary")
    static EWarriorHitResultType EvaluateHitResult(AActor* InAttacker, AActor* InVictim, AActor* InDamageCauser = nullptr, EWarriorBlockRule InBlockRule = EWarriorBlockRule::Blockable);

    // 사망 상태(Shared.Status.Death 하위 태그)인지. ASC가 없거나 이미 제거 중인 액터도 안전하게 처리 (false 또는 true)
    UFUNCTION(BlueprintPure, Category = "Warrior|FunctionLibrary")
    static bool IsActorDead(AActor* InActor);

    // 폰 회전 설정. ALS는 TargetRotation으로 되돌리려 하고, LookingDirection 모드는 컨트롤 회전과 어긋나면
    // 제자리 회전으로 되돌리므로 둘 다 함께 갱신
    static void SetPawnFacingRotation(APawn* InPawn, const FRotator& NewRotation);

};
