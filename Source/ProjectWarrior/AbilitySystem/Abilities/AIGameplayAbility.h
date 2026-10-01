// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameplayAbility.h"
#include "AIGameplayAbility.generated.h"

class AWarriorAICharacter;
class UAICombatComponent;
class AWarriorProjectileBase;

/**
 * 
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility : public UWarriorGameplayAbility
{
	GENERATED_BODY()
	
public:
    UAIGameplayAbility();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    AWarriorAICharacter* GetAICharacterFromActorInfo();

    // 대상에게 보낼 히트리액션 이벤트 태그. 이벤트 데이터(MeleeHit 등)에 무기 충돌 노티파이가 지정한 강도가 있으면 그것을,
    // 없으면 이 어빌리티의 HitReactEventTag를 반환
    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    FGameplayTag ResolveHitReactEventTag(const FGameplayEventData& InPayload) const;

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    UAICombatComponent* GetAICombatComponentFromActorInfo();

    UFUNCTION(BlueprintPure, Category = "Warrior|Ability")
    FGameplayEffectSpecHandle MakeAIDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, const FScalableFloat& InDamageScalableFloat);

    // 장착 무기의 소켓에서 대상 방향으로 투사체를 스폰. TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터를 사용
    UFUNCTION(BlueprintCallable, Category = "Warrior|Ability", meta = (DeterminesOutputType = "ProjectileClass"))
    AWarriorProjectileBase* SpawnProjectileFromEquippedWeapon(TSubclassOf<AWarriorProjectileBase> ProjectileClass, FName SpawnSocketName, AActor* TargetActor, const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bPredictTargetMovement = true);

protected:
    // 기본 히트리액션 강도 (Shared.Event.HitReact.Light/Heavy). 근접 공격은 무기 충돌 노티파이 값이 우선
    // 플레이어는 강도에 따라 다르게 반응하고, 적은 강도와 무관하게 반응함
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact", meta = (Categories = "Shared.Event.HitReact"))
    FGameplayTag HitReactEventTag;

    // 장착 무기 메시(없거나 bUseCharacterMesh면 캐릭터 메시)를 반환
    USceneComponent* GetProjectileSocketParent(bool bUseCharacterMesh);

    // 대기 상태의 투사체를 스폰만 함 (발사는 AWarriorProjectileBase::LaunchProjectile)
    AWarriorProjectileBase* SpawnUnlaunchedProjectile(TSubclassOf<AWarriorProjectileBase> ProjectileClass, const FTransform& SpawnTransform);

    // TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터 반환
    AActor* ResolveProjectileTarget(AActor* TargetActor);

    // TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터 사용. 대상이 없으면 캐릭터 정면
    FVector ComputeProjectileLaunchDirection(const FVector& LaunchLocation, AActor* TargetActor, float ProjectileSpeed, bool bPredictTargetMovement);

private:
    TWeakObjectPtr<AWarriorAICharacter> CachedAICharacter;
};
