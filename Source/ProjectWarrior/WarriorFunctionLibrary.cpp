// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorFunctionLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/Interfaces/PawnCombatInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "GenericTeamAgentInterface.h"

UWarriorAbilitySystemComponent* UWarriorFunctionLibrary::NativeGetWarriorASCFromActor(AActor* InActor)
{
    check(InActor);

    return CastChecked<UWarriorAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor));
}

void UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd)
{
    UWarriorAbilitySystemComponent* ASC = NativeGetWarriorASCFromActor(InActor);

    if (!ASC->HasMatchingGameplayTag(TagToAdd))
    {
        ASC->AddLooseGameplayTag(TagToAdd);
    }
}

void UWarriorFunctionLibrary::RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove)
{
    UWarriorAbilitySystemComponent* ASC = NativeGetWarriorASCFromActor(InActor);

    if (ASC->HasMatchingGameplayTag(TagToRemove))
    {
        ASC->RemoveLooseGameplayTag(TagToRemove);
    }
}

bool UWarriorFunctionLibrary::NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck)
{
    UWarriorAbilitySystemComponent* ASC = NativeGetWarriorASCFromActor(InActor);

    return ASC->HasMatchingGameplayTag(TagToCheck);
}

void UWarriorFunctionLibrary::BP_DoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck, EWarriorConfirmType& OutConfirmType)
{
    OutConfirmType = NativeDoesActorHaveTag(InActor, TagToCheck) ? EWarriorConfirmType::Yes : EWarriorConfirmType::No;
}

bool UWarriorFunctionLibrary::NativeDoesActorHaveAnyTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck)
{
    UWarriorAbilitySystemComponent* ASC = NativeGetWarriorASCFromActor(InActor);

    return ASC->HasAnyMatchingGameplayTags(TagContainerToCheck);
}

void UWarriorFunctionLibrary::BP_DoesActorHaveAnyTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck, EWarriorConfirmType& OutConfirmType)
{
    OutConfirmType = NativeDoesActorHaveAnyTag(InActor, TagContainerToCheck) ? EWarriorConfirmType::Yes : EWarriorConfirmType::No;
}

bool UWarriorFunctionLibrary::NativeDoesActorHaveAllTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck)
{
    UWarriorAbilitySystemComponent* ASC = NativeGetWarriorASCFromActor(InActor);

    return ASC->HasAllMatchingGameplayTags(TagContainerToCheck);
}

void UWarriorFunctionLibrary::BP_DoesActorHaveAllTag(AActor* InActor, FGameplayTagContainer TagContainerToCheck, EWarriorConfirmType& OutConfirmType)
{
    OutConfirmType = NativeDoesActorHaveAllTag(InActor, TagContainerToCheck) ? EWarriorConfirmType::Yes : EWarriorConfirmType::No;
}

UPawnCombatComponent* UWarriorFunctionLibrary::NativeGetPawnCombatComponentFromActor(AActor* InActor)
{
    check(InActor);

    if (IPawnCombatInterface* PawnCombatInterface = Cast<IPawnCombatInterface>(InActor))
    {
        return PawnCombatInterface->GetPawnCombatComponent();
    }

    return nullptr;
}

UPawnCombatComponent* UWarriorFunctionLibrary::BP_GetPawnCombatComponentFromActor(AActor* InActor, EWarriorValidType& OutValidType)
{
    UPawnCombatComponent* CombatComponent = NativeGetPawnCombatComponentFromActor(InActor);

    OutValidType = CombatComponent ? EWarriorValidType::Valid : EWarriorValidType::Invalid;

    return CombatComponent;
}

FGameplayTag UWarriorFunctionLibrary::ComputeHitReactDirectionTag(AActor* InAttacker, AActor* InVictim, float& OutAngleDifference)
{
    check(InAttacker && InVictim);

    const FVector VictimForward = InVictim->GetActorForwardVector();
    const FVector VictimToAttackerNormalized = (InAttacker->GetActorLocation() - InVictim->GetActorLocation()).GetSafeNormal();

    //내적을 통해 각도를 계산
    const float DotResult = FVector::DotProduct(VictimForward, VictimToAttackerNormalized);
    OutAngleDifference = UKismetMathLibrary::DegAcos(DotResult);

    //외적을 통해 수직인 벡터를 구하고 이를 통해 왼쪽, 오른쪽을 구분함.
    const FVector CrossResult = FVector::CrossProduct(VictimForward, VictimToAttackerNormalized);

    if (CrossResult.Z < 0.f)
    {
        OutAngleDifference *= -1.f;
    }

    if (OutAngleDifference >= -45.f && OutAngleDifference <= 45.f)
    {
        return WarriorGameplayTags::Shared_Status_HitReact_Front;
    }
    else if (OutAngleDifference < -45.f && OutAngleDifference >= -135.f)
    {
        return WarriorGameplayTags::Shared_Status_HitReact_Left;
    }
    else if (OutAngleDifference < -135.f || OutAngleDifference > 135.f)
    {
        return WarriorGameplayTags::Shared_Status_HitReact_Back;
    }
    else if (OutAngleDifference > 45.f && OutAngleDifference <= 135.f)
    {
        return WarriorGameplayTags::Shared_Status_HitReact_Right;
    }

    return WarriorGameplayTags::Shared_Status_HitReact_Front;
}

bool UWarriorFunctionLibrary::IsValidBlock(AActor* InAttacker, AActor* InDefender)
{
    check(InAttacker && InDefender);

    const float DotResult = FVector::DotProduct(InAttacker->GetActorForwardVector(), InDefender->GetActorForwardVector());

    return DotResult < -0.1f ? true : false;
}

bool UWarriorFunctionLibrary::IsActorDead(AActor* InActor)
{
    if (!IsValid(InActor))
    {
        // 이미 제거된 대상은 죽은 것으로 취급
        return true;
    }

    const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor);

    return ASC && ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Death);
}

EWarriorHitResultType UWarriorFunctionLibrary::EvaluateHitResult(AActor* InAttacker, AActor* InVictim, AActor* InDamageCauser, bool bIsAttackUnblockable)
{
    check(InAttacker && InVictim);

    // 처형 연출 중이거나 이미 죽은 대상은 판정하지 않음 (투사체·범위 공격 포함)
    if (NativeDoesActorHaveTag(InVictim, WarriorGameplayTags::Shared_Status_Finisher) || IsActorDead(InVictim))
    {
        return EWarriorHitResultType::Invalid;
    }

    const bool bIsVictimBlocking = NativeDoesActorHaveTag(InVictim, WarriorGameplayTags::Player_Status_Blocking);

    if (bIsVictimBlocking && !bIsAttackUnblockable && IsValidBlock(InDamageCauser ? InDamageCauser : InAttacker, InVictim))
    {
        return EWarriorHitResultType::Blocked;
    }

    if (NativeDoesActorHaveTag(InVictim, WarriorGameplayTags::Shared_Status_Dodge))
    {
        return EWarriorHitResultType::Dodged;
    }

    return EWarriorHitResultType::Hit;
}

bool UWarriorFunctionLibrary::IsTargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn)
{
    check(QueryPawn && TargetPawn);

    IGenericTeamAgentInterface* QueryTeamAgent = Cast<IGenericTeamAgentInterface>(QueryPawn->GetController());
    IGenericTeamAgentInterface* TargetTeamAgent = Cast<IGenericTeamAgentInterface>(TargetPawn->GetController());

    if (QueryTeamAgent && TargetTeamAgent)
    {
        return QueryTeamAgent->GetGenericTeamId() != TargetTeamAgent->GetGenericTeamId();
    }

    return false;
}
