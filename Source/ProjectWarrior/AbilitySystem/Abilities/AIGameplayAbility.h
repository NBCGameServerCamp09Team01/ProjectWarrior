// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameplayAbility.h"
#include "AIGameplayAbility.generated.h"

class AWarriorAICharacter;
class UAICombatComponent;
class UAttackTokenComponent;
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
    //~ Begin GameplayAbility Interface.
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    //~ End GameplayAbility Interface

    // 공격 대상에게서 받을 공격 토큰 풀 (AI.AttackToken.*). 비어 있거나 보스가 발동하면 토큰 없이 발동
    // 토큰이 없으면 발동에 실패하고, 어빌리티가 끝나면(취소 포함) 반납함
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AttackToken", meta = (Categories = "AI.AttackToken"))
    FGameplayTag AttackTokenPool;

    // 이 공격이 차지하는 토큰 수. 강공격은 2로 두면 그동안 다른 적이 덜 공격함
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AttackToken", meta = (ClampMin = "1"))
    int32 AttackTokenCost = 1;

    // 정상 종료(취소 아님)하면 이어서 발동할 어빌리티 (예: 밀치기 -> 백스텝 -> 속사). 발동하지 못하면(토큰 부족 등) 무시
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FollowUp", meta = (Categories = "AI.Ability"))
    FGameplayTag FollowUpAbilityTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FollowUp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FollowUpChance = 1.f;

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

    // 처형(AI.Event.Finisher, Shared.Status.Finisher)이나 사망(Shared.Status.Death)이 시작되면 이 어빌리티를 취소하도록 감시
    // 회피·가드처럼 반응 지연이나 대기 중에 처형 몽타주를 덮어쓰면 안 되는 어빌리티의 ActivateAbility에서 호출
    void WatchForFinisherOrDeath();

    // 피격 경직·스태거·처형·사망 중이면 true (회피·가드를 시작하지 않음)
    static bool IsOwnerIncapacitated(const UAbilitySystemComponent* ASC);

    // AI 컨트롤러 블랙보드의 TargetActor, 없으면 Focus 액터. 없으면 nullptr
    static AActor* FindAITargetActor(const FGameplayAbilityActorInfo* ActorInfo);

private:
    UFUNCTION()
    void HandleFinisherOrDeathEvent(FGameplayEventData Payload);

    UFUNCTION()
    void HandleFinisherOrDeathTag();

    void CancelForFinisherOrDeath();

    // 토큰 풀이 지정돼 있고 보스가 아니면 true
    bool UsesAttackToken(const FGameplayAbilityActorInfo* ActorInfo) const;

    // FindAITargetActor가 가진 토큰 컴포넌트. 대상이 토큰을 쓰지 않으면 nullptr
    static UAttackTokenComponent* FindTargetAttackTokenComponent(const FGameplayAbilityActorInfo* ActorInfo);

    TWeakObjectPtr<AWarriorAICharacter> CachedAICharacter;

    // 이번 발동에서 토큰을 받은 컴포넌트. EndAbility에서 반납
    TWeakObjectPtr<UAttackTokenComponent> HeldAttackTokenComponent;
};
