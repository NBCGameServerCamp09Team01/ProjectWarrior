// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGameMode.h"
#include "GameplayTagContainer.h"
#include "ProjectWarrior/Tutorial/WarriorTutorialTypes.h"
#include "WarriorTutorialGameMode.generated.h"

class AWarriorTutorialZone;
class UAbilitySystemComponent;
class UDataAsset_TutorialSteps;
class UGameplayAbility;
class UPlayerInventoryComponent;
struct FGameplayEventData;

//진행 상태·단계·진행 횟수·힌트가 바뀌었다. 화면은 게터로 다시 읽는다
DECLARE_MULTICAST_DELEGATE(FOnWarriorTutorialChanged);
//막힌 어빌리티를 쓰려 했다(아직 배우지 않은 조작)
DECLARE_MULTICAST_DELEGATE(FOnWarriorTutorialAbilityBlocked);

/**
 * 튜토리얼 레벨(L_Tutorial)의 GameMode(계획서 4장). 스테이지 판을 만들지 않는다.
 * - 서버(P1·R1·C1·P4)를 부르지 않고, 판 통계는 스테이지 GameMode가 아니라서 기록되지 않는다(UWarriorStageStatsSubsystem::OnWorldBeginPlay).
 * - 장 번호는 레벨 열기 옵션 Chapter=N(0부터)으로 받는다. 없으면 처음부터.
 * - 아직 배우지 않은 어빌리티는 엔진의 BlockAbilitiesWithTags로 막는다. 본 게임은 이 함수를 부르지 않는다.
 * - 완료 신호는 플레이어 ASC의 콜백(어빌리티 활성·게임플레이 이벤트·태그)을 듣는다. 어빌리티는 고치지 않는다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorTutorialGameMode : public AWarriorGameMode
{
	GENERATED_BODY()

public:
	AWarriorTutorialGameMode();

	//레벨 열기 옵션 이름(Chapter=N)
	static const TCHAR* ChapterOption;

	EWarriorTutorialState GetTutorialState() const { return TutorialState; }
	const UDataAsset_TutorialSteps* GetTutorialSteps() const { return TutorialSteps; }
	int32 GetChapterIndex() const { return ChapterIndex; }
	int32 GetStepIndex() const { return StepIndex; }
	//지금 단계. 진행 중이 아니면 nullptr
	const FWarriorTutorialStep* GetCurrentStep() const;
	int32 GetStepProgress() const { return StepProgress; }
	bool IsHintShown() const { return bHintShown; }

	//힌트를 보인 단계가 진행 중이면 true. 튜토리얼 전용 공격 몽타주의 슬로우 구간(UAnimNotifyState_WarriorTutorialSlow)이 본다
	bool IsSlowHintActive() const { return TutorialState == EWarriorTutorialState::StepActive && bHintShown; }

	//지금 단계를 끝내지 않고 다음 단계로 간다(일시정지 메뉴 "이 단계 건너뛰기")
	void SkipCurrentStep();
	bool CanSkipCurrentStep() const;

	FOnWarriorTutorialChanged OnTutorialChanged;
	FOnWarriorTutorialAbilityBlocked OnAbilityBlocked;

protected:
	//~ Begin AActor Interface.
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin AGameModeBase Interface.
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	//~ End AGameModeBase Interface

	//장·단계 목록(BP_TutorialGameMode에서 DA_TutorialSteps 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial")
	TObjectPtr<UDataAsset_TutorialSteps> TutorialSteps;

	//단계 문구를 보인 뒤 신호를 듣기 시작할 때까지(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "0.0", Units = "s"))
	float StepIntroSeconds = 1.f;

	//완료 표시를 보이는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "0.0", Units = "s"))
	float StepDoneSeconds = 1.5f;

	//막힌 어빌리티 알림을 다시 보내기까지(초). 키를 누르고 있으면 매 프레임 실패가 온다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "0.0", Units = "s"))
	float BlockedNoticeInterval = 1.f;

private:
	void StartTutorial(APawn* InPlayerPawn);
	void EnterStep(int32 InChapterIndex, int32 InStepIndex);
	void BeginStepActive();
	void AddStepProgress();
	void CompleteStep();
	void AdvanceStep();
	void FinishTutorial();
	void SetTutorialState(EWarriorTutorialState InState);

	//단계에 맞게 막을 어빌리티를 정하고, 지금 막은 것과 다른 것만 막거나 푼다(엔진 차단은 카운트로 쌓인다)
	void ApplyAbilityBlocks(const FGameplayTagContainer& InDesiredBlocked);

	//지금 단계의 완료·실패 신호를 듣는다/그만 듣는다
	void BindStepSignals(const FWarriorTutorialStep& InStep);
	void UnbindStepSignals();

	void ActivateStepZone(const FWarriorTutorialStep& InStep);
	void GrantStepItems(const FWarriorTutorialStep& InStep);
	bool HasRequiredOwnedTag(const FWarriorTutorialStep& InStep) const;

	//~ Begin 신호
	void HandleAbilityActivated(UGameplayAbility* InAbility);
	void HandleAbilityFailed(const UGameplayAbility* InAbility, const FGameplayTagContainer& InFailureTags);
	void HandleGoalEvent(const FGameplayEventData* InPayload);
	void HandleFailureEvent(const FGameplayEventData* InPayload);
	void HandleGoalTagChanged(const FGameplayTag InTag, int32 InNewCount);
	void HandleZoneReached(AWarriorTutorialZone* InZone);
	UFUNCTION()
	void HandleInventoryChanged();
	//~ End 신호

	UPlayerInventoryComponent* GetPlayerInventory() const;

	EWarriorTutorialState TutorialState = EWarriorTutorialState::Loading;
	int32 StartChapterIndex = 0;
	int32 ChapterIndex = INDEX_NONE;
	int32 StepIndex = INDEX_NONE;
	int32 StepProgress = 0;
	int32 StepFailures = 0;
	bool bHintShown = false;

	//InputUsed: 들어온 시간의 합, 지난 틱의 입력 여부(누른 순간 세기)
	float StepInputSeconds = 0.f;
	bool bStepInputWasActive = false;

	//ItemUsed: 단계 시작 때의 개수. 아이템은 단계 데이터(DataAsset)가 잡고 있다
	TMap<const UDataAsset_Item*, int32> StepItemCounts;

	TWeakObjectPtr<APawn> PlayerPawn;
	TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;

	//지금 걸어 둔 차단. 끝날 때 이만큼 푼다
	FGameplayTagContainer AppliedBlockedTags;
	//붙인 임시 해금 태그. 끝날 때 뗀다
	FGameplayTagContainer AppliedSkillTags;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AWarriorTutorialZone>> Zones;
	TWeakObjectPtr<AWarriorTutorialZone> ActiveZone;

	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AbilityFailedHandle;
	FDelegateHandle GoalSignalHandle;
	FDelegateHandle FailureSignalHandle;
	FDelegateHandle ZoneReachedHandle;
	FGameplayTag BoundGoalTag;
	FGameplayTag BoundFailureTag;
	EWarriorTutorialGoal BoundGoal = EWarriorTutorialGoal::AbilityActivated;
	bool bInventoryBound = false;

	FTimerHandle StepTimer;
	double LastBlockedNoticeTime = -1.0;
};
