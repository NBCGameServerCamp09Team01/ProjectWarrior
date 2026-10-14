// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayerGameplayAbility.h"
#include "PlayerGameplayAbility_TacticalMode.generated.h"

class UInputMappingContext;
class UInputAction;
class UPlayerGameplayAbility_TacticalTechnique;
class UTacticalMenuWidget;
struct FInputActionValue;
struct FAbilityEndedData;

UENUM()
enum class ETacticalModeStep : uint8
{
	SelectCommand,
	SelectTarget,
	Executing
};

// 행동 스톡 한 칸: 어떤 기술을 누구에게
USTRUCT()
struct FTacticalStock
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<UPlayerGameplayAbility_TacticalTechnique> Technique;

	TWeakObjectPtr<AActor> Target;
};

/**
 * 전술 모드(FF7 리메이크). T로 켜면 플레이어를 포함한 세상이 느려지고 커맨드 메뉴가 뜬다.
 * W/S로 기술 고름 → 좌클릭 → 같은 패널에 범위 안 대상 목록, A/D로 고름(카메라가 대상으로 돎) → 좌클릭 = 스톡 한 칸.
 * 우클릭은 한 단계 뒤로(스톡이 없으면 닫기). 고르는 동안 이동·시점·회피 등 다른 입력은 막는다(IMC_TacticalMode가 키를 가로챔).
 * 스톡이 다 차거나(최대 3, 남은 스태미나로 넣을 기술이 없을 때 포함) 좌클릭을 더블탭하면 시간을 되돌리고 스톡을 차례로 실행한다.
 * 단일 좌클릭은 더블탭인지 DoubleTapWindow만큼 기다렸다가 처리한다.
 */
UCLASS()
class PROJECTWARRIOR_API UPlayerGameplayAbility_TacticalMode : public UPlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerGameplayAbility_TacticalMode();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnTacticalTick(float DeltaTime);

	void OnUpInput(const FInputActionValue& InputActionValue);
	void OnDownInput(const FInputActionValue& InputActionValue);
	void OnLeftInput(const FInputActionValue& InputActionValue);
	void OnRightInput(const FInputActionValue& InputActionValue);
	void OnConfirmInput(const FInputActionValue& InputActionValue);
	void OnBackInput(const FInputActionValue& InputActionValue);

	// 기술 선택은 W/S, 대상 선택은 A/D(대상은 화면 왼쪽부터 정렬)
	void MoveCommandSelection(int32 InDelta);
	void MoveTargetSelection(int32 InDelta);
	void HandleConfirm();

	void BeginSelection();
	void EndSelection();

	// 범위 안의 적을 화면 왼쪽부터 정렬
	void GatherTargets();

	// 적대·살아 있음·처형당하는 중 아님·초기화 중 아님
	bool IsValidTarget(AActor* InActor);

	// 고르는 동안 죽거나 처형당하는 적을 대상 목록과 스톡에서 뺌
	void PruneInvalidTargets();

	void GrantTechniques();
	float GetTechniqueCost(const TSubclassOf<UPlayerGameplayAbility_TacticalTechnique>& InTechnique) const;
	float GetRemainingStamina();
	bool CanAffordAnyTechnique();

	void RefreshMenu();

	// 대상을 고르는 동안 카메라를 그 대상 쪽으로 돌림
	void TickTargetFocus(float RealDeltaTime);

	void ExecuteStocks();

	// 피니시 도중이면 끝날 때까지 실행을 미룸
	bool IsWaitingForFinisher();
	void BeginExecution();

	void RunNextStock();
	void OnAbilityEnded(const FAbilityEndedData& AbilityEndedData);

	// 실행 중 적을 멈춰 두고(맞은 적은 잠깐 풀어 줌), 끝나면 원래 배율로 되돌림
	void TickExecutionFreeze();
	void RestoreFrozenActors();

	UFUNCTION()
	void OnExecutionMeleeHit(FGameplayEventData Payload);

#pragma region Commands
	// 커맨드 메뉴에 나오는 기술(위에서부터)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode")
	TArray<TSubclassOf<UPlayerGameplayAbility_TacticalTechnique>> Techniques;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode", meta = (ClampMin = "1"))
	int32 MaxStocks = 3;

	// 대상으로 고를 수 있는 거리(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode", meta = (ClampMin = "0.0"))
	float TargetRange = 1500.0f;

	// 세상과 플레이어의 시간 배율
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TimeDilation = 0.1f;

	// 좌클릭 두 번을 더블탭으로 보는 간격(실제 시간, 초)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode", meta = (ClampMin = "0.05"))
	float DoubleTapWindow = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode")
	TSubclassOf<UTacticalMenuWidget> MenuWidgetClass;

	// 실행하는 동안 적(과 적이 쏜 투사체)의 시간 배율. 세상 시간은 1배라 플레이어만 움직이는 턴제 느낌
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Execution", meta = (ClampMin = "0.0001", ClampMax = "1.0"))
	float ExecutionEnemyTimeDilation = 0.001f;

	// 맞은 적을 피격 모션이 나오도록 잠깐 풀어 두는 시간(초). 죽은 적은 계속 풀어 둠
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Execution", meta = (ClampMin = "0.0"))
	float HitUnfreezeDuration = 0.8f;

	// 대상을 고르는 동안 카메라를 그 대상으로 돌리는 속도(실제 시간)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Camera", meta = (ClampMin = "0.0"))
	float TargetFocusInterpSpeed = 6.0f;

	// 대상을 볼 때 카메라를 얼마나 내려다볼지(도, 음수 = 아래)
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Camera")
	float TargetFocusPitch = -12.0f;
#pragma endregion

#pragma region Input
	// 전술 모드 동안만 켜는 매핑. W/S·좌클릭·우클릭을 기본 이동·공격·막기보다 먼저 가로챔
	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputMappingContext> TacticalMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	int32 TacticalMappingPriority = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputAction> UpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputAction> DownAction;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputAction> LeftAction;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputAction> RightAction;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(EditDefaultsOnly, Category = "Tactical Mode|Input")
	TObjectPtr<UInputAction> BackAction;
#pragma endregion

	UPROPERTY(Transient)
	TObjectPtr<UTacticalMenuWidget> MenuWidget;

	UPROPERTY(Transient)
	TArray<FTacticalStock> Stocks;

	TArray<TWeakObjectPtr<AActor>> Targets;

	// Targets와 같은 순서. 같은 이름이 여럿이면 "이름 (1)"처럼 번호를 붙임
	TArray<FText> TargetNames;

	ETacticalModeStep Step = ETacticalModeStep::SelectCommand;
	int32 CommandIndex = 0;
	int32 TargetIndex = 0;
	int32 ExecutingIndex = INDEX_NONE;

	FGameplayAbilitySpecHandle ExecutingTechniqueHandle;
	FDelegateHandle AbilityEndedHandle;

	// 기다리는 단일 좌클릭의 시각(실제 시간). 음수면 없음
	double PendingConfirmTime = -1.0;

	TArray<uint32> InputBindingHandles;
	float SavedPlayerTimeDilation = 1.0f;

	// 실행 중 멈춘 적과 원래 시간 배율
	TMap<TWeakObjectPtr<AActor>, float> FrozenActorSavedDilations;

	// 맞아서 풀어 둔 적과 다시 멈출 실제 시각
	TMap<TWeakObjectPtr<AActor>, double> UnfrozenUntil;

	// 고르는 동안 모션 블러를 끔(세상이 느린데 카메라만 실제 시간으로 돌면 블러가 길게 번짐)
	TWeakObjectPtr<class UCameraComponent> CachedCamera;
	bool bSavedOverrideMotionBlur = false;
	float SavedMotionBlurAmount = 0.0f;
	bool bSelectionApplied = false;

	// 실행을 눌렀을 때 피니시 도중이어서 끝나길 기다리는 중
	bool bWaitingForFinisher = false;
};
