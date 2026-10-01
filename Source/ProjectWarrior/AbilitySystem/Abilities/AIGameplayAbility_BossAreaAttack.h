// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIGameplayAbility.h"
#include "ProjectWarrior/Types/WarriorAttackAreaTypes.h"
#include "AIGameplayAbility_BossAreaAttack.generated.h"

class AWarriorAttackIndicator;
class UAnimMontage;

/**
 * 위험 범위를 표시한 뒤 그 영역에 판정하는 보스 범위 공격.
 * 사용 흐름 (BP): 몽타주 재생 -> AI.Event.Boss.Telegraph 수신 시 BeginAreaTelegraph
 *               -> AI.Event.Boss.AreaImpact 수신 시 ApplyAreaDamage
 * 표시와 판정이 같은 FWarriorAttackAreaData / 영역 트랜스폼을 사용하므로 보이는 범위 = 맞는 범위.
 */
UCLASS()
class PROJECTWARRIOR_API UAIGameplayAbility_BossAreaAttack : public UAIGameplayAbility
{
	GENERATED_BODY()

public:
	UAIGameplayAbility_BossAreaAttack();

protected:
	//~ Begin GameplayAbility Interface.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End GameplayAbility Interface

	// 영역 위치를 확정하고 위험 범위를 표시. 이미 표시 중이면 교체
	// TargetActor가 비어 있으면 AI 컨트롤러의 Focus 액터, 그것도 없으면 블랙보드 TargetActor 사용
	// (Anchor가 TargetSnapshot인데 대상을 찾지 못하면 공격자 기준 + 경고 로그)
	// TelegraphDuration은 표시가 가득 차는 시간 (판정 시점까지 남은 시간과 맞춤)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	void BeginAreaTelegraph(const FWarriorAttackAreaData& InAreaData, AActor* TargetActor, float TelegraphDuration);

	// DefaultAreaData로 BeginAreaTelegraph
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	void BeginDefaultAreaTelegraph(AActor* TargetActor, float TelegraphDuration);

	// 현재 영역 안의 적대 대상에게 판정. 피격(Hit)된 대상 목록 반환. 영역이 없으면 빈 목록
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	TArray<AActor*> ApplyAreaDamage(const FGameplayEffectSpecHandle& InDamageSpecHandle, bool bClearTelegraph = true);

	// 루트 모션으로 이동하는 몽타주용. 몽타주의 ImpactEventTag 노티파이 시점까지의 루트 모션을 미리 계산해,
	// 그 시점의 보스 위치·방향에 DefaultAreaData 영역을 고정 표시 (Anchor 설정과 무관하게 고정)
	// 몽타주 재생 전에 호출하면 처음부터, 재생 중에 호출하면 현재 위치부터 계산
	// bFaceTarget: 계산 전에 대상 방향으로 즉시 회전 (TargetActor가 비어 있으면 Focus -> 블랙보드 TargetActor)
	// TelegraphDuration < 0 이면 판정 시점까지 남은 시간으로 자동 설정. 노티파이를 찾지 못하면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack", meta = (AdvancedDisplay = "TelegraphDuration"))
	bool BeginMontageAreaTelegraph(UAnimMontage* Montage, AActor* TargetActor = nullptr, bool bFaceTarget = true, float TelegraphDuration = -1.f);

	// 대상 방향으로 즉시 회전 (ALS 목표 회전·컨트롤 회전 포함). 대상을 찾지 못하면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	bool FaceAreaTarget(AActor* TargetActor = nullptr);

	// 표시를 제거하고 영역을 비움
	UFUNCTION(BlueprintCallable, Category = "Warrior|Ability|AreaAttack")
	void ClearAreaTelegraph();

	UFUNCTION(BlueprintPure, Category = "Warrior|Ability|AreaAttack")
	bool HasActiveArea() const { return bHasActiveArea; }

	// Owner 앵커면 현재 공격자 기준, 그 외에는 고정된 트랜스폼
	UFUNCTION(BlueprintPure, Category = "Warrior|Ability|AreaAttack")
	FTransform GetCurrentAreaTransform() const;

	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	FWarriorAttackAreaData DefaultAreaData;

	// 비워 두면 표시 없이 판정만 함
	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	TSubclassOf<AWarriorAttackIndicator> IndicatorClass;

	// 피격 대상에게 HitReactEventTag(부모 UAIGameplayAbility, 범위 공격 기본 Heavy)를 보낼지
	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	bool bSendHitReactEvent = true;

	// BeginMontageAreaTelegraph가 판정 시점으로 찾을 노티파이의 이벤트 태그
	// (노티파이 객체에 이 값을 가진 GameplayTag 변수가 있으면 해당 노티파이로 인식. 예: AN_SendGameplayEventToOwner의 Event Tag)
	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack")
	FGameplayTag MontageImpactEventTag;

	UPROPERTY(EditDefaultsOnly, Category = "AreaAttack|Debug")
	bool bDrawDebugArea = false;

	// 대상 우선순위: 직접 넘긴 대상 -> AI 컨트롤러 Focus -> 블랙보드 TargetActor. 없으면 nullptr
	AActor* ResolveAreaTarget(AActor* TargetActor);

	// 앵커 트랜스폼(오프셋 적용 전)을 직접 지정해 위험 범위를 표시. 이미 표시 중이면 교체
	void BeginAreaTelegraphAtAnchor(const FWarriorAttackAreaData& InAreaData, const FTransform& AnchorTransform, float TelegraphDuration);

	// 지정한 영역·트랜스폼(오프셋 적용 후)으로 즉시 판정. 피격(Hit)된 대상 목록 반환
	// InOutProcessedActors가 있으면 이미 들어 있는 대상은 건너뛰고, 판정한 대상(막기·회피 포함)을 추가 (연속 판정에서 1회만 맞게)
	TArray<AActor*> ApplyAreaDamageAt(const FWarriorAttackAreaData& InAreaData, const FTransform& InAreaTransform, const FGameplayEffectSpecHandle& InDamageSpecHandle, TSet<TWeakObjectPtr<AActor>>* InOutProcessedActors = nullptr);

	// InDesiredPoint를 내비메시 위로 투영하고, InStartPoint -> 목표 사이가 끊겨 있으면(벽, 낭떠러지) 끊긴 지점까지로 줄임
	// 내비게이션이 없거나 투영 실패 시 InDesiredPoint 그대로
	FVector AdjustPointToNavigation(const FVector& InStartPoint, const FVector& InDesiredPoint, const FVector& InProjectExtent) const;

	// 공격자 회전 설정. ALS는 TargetRotation으로 되돌리려 하고, LookingDirection 모드는 컨트롤 회전과 어긋나면
	// 제자리 회전으로 되돌리므로 둘 다 함께 갱신
	void SetOwnerFacingRotation(const FRotator& NewRotation) const;

	// 몽타주 노티파이 중 InEventTag 값을 가진 GameplayTag 변수를 가진 첫 노티파이의 시간
	static bool FindMontageEventTime(const UAnimMontage* InMontage, const FGameplayTag& InEventTag, float& OutTime);

private:
	// 앵커 기준(오프셋 적용 전) 트랜스폼. 수평 방향만 사용
	FTransform ComputeAnchorTransform(EWarriorAttackAreaAnchor InAnchor, AActor* TargetActor);

	FWarriorAttackAreaData CurrentAreaData;

	// Owner 앵커가 아닐 때 고정된 영역 트랜스폼
	FTransform SnapshotAreaTransform;

	bool bHasActiveArea = false;

	TWeakObjectPtr<AWarriorAttackIndicator> ActiveIndicator;
};
