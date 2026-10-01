// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorPlayerController.h"
#include "Components/SlateWrapperTypes.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStagePlayerController.generated.h"

class AWarriorStageGameState;
class UUserWidget;
class UWarriorStageHUDWidget;
class UWarriorStageResultWidget;

/**
 * 스테이지 레벨의 PlayerController.
 * 스테이지 상태가 바뀔 때마다 GameState의 상태별 권한 표(AWarriorStageGameState::GetPermission)를
 * 이동 잠금·시점 잠금·입력 모드·커서에 적용한다.
 * 스테이지 HUD를 만들고, 끝 상태(StageCleared·StageFailed)에서는 결과 화면을 띄운다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorStagePlayerController : public AWarriorPlayerController
{
	GENERATED_BODY()

public:
	//[기술부채] 플레이어 사망 어빌리티(GA_Player_Death)의 종료를 이 두 함수의 레벨 이동에 기대고 있다.
	//현재: GA_Player_Death는 쓰러진 자세 유지와 피격 반응 차단(Block 태그)을 위해 정상 경로에서 EndAbility를 부르지 않는다.
	//      실패 상태에서 나가는 길은 RestartStage·ReturnToMainMenu(둘 다 OpenLevel)뿐이고, 이때 캐릭터와 함께 ASC가 파괴되며
	//      GAS가 CancelAbilities·ClearAllAbilities로 어빌리티를 취소·종료한다(UAbilitySystemComponent::DestroyActiveState).
	//오류 여지: 아래처럼 바뀌면 사망 어빌리티와 Block 태그가 남아 공격·회피·막기 등이 영원히 막힌다.
	//      - 같은 레벨 안에서 부활·이어 하기·리스폰을 추가할 때
	//      - ASC를 캐릭터가 아닌 PlayerState로 옮길 때
	//      - 리슨 서버로 확장할 때(클라이언트는 파괴 때 EndAbility 없이 인스턴스만 정리됨)
	//      - "사망 어빌리티가 끝나면" 무언가를 하는 기능을 추가할 때(그 알림이 오지 않음)
	//      그때는 GA_Player_Death가 몽타주 끝에서 EndAbility를 부르고, 피격 반응 등은 사망 상태 태그(Shared.Status.Death)로 막도록 바꾼다.

	//현재 스테이지 레벨을 다시 연다 (결과 화면 "다시 하기")
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RestartStage();

	//MainMenuLevel로 이동한다 (결과 화면 "메인메뉴")
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void ReturnToMainMenu();

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//GameState의 OnStageStateChanged에 연결
	UFUNCTION()
	void HandleStageStateChanged(EWarriorStageState InNewState, EWarriorStageState InOldState);

	//InState의 권한을 적용한다.
	//입력 모드·시점·커서는 UI 입력 모드 값이 바뀔 때만 적용하고, bForceInputMode가 true면 같아도 다시 적용한다.
	//InFocusWidget은 UI 입력 모드로 바꿀 때 포커스를 줄 위젯
	void ApplyStatePermission(EWarriorStageState InState, bool bForceInputMode = false, UUserWidget* InFocusWidget = nullptr);

	//GameState의 OnStageFinished에 연결. 상태가 끝 상태로 바뀌기 직전에 온다
	UFUNCTION()
	void HandleStageFinished(const FWarriorStageResult& InResult);

	//GameState의 OnWaveChanged에 연결. 웨이브는 InProgress에 들어갈 때만 바뀐다(웨이브 0은 초기화 알림)
	UFUNCTION()
	void HandleWaveChanged(int32 InWaveNumber, int32 InTotalWaveCount, bool bInBossWave);

	//상태에 맞는 음악 상황을 알리고 상태 전환 소리를 낸다. 결과 음악은 ShowResult·RevealResult가 맡는다
	void PlayStageStateSound(EWarriorStageState InState);

	//결과 위젯을 만들어(처음 한 번) GameState가 확정한 결과를 넘기고 띄운다.
	//실패이고 FailedResultDelay가 있으면 위젯은 지금 만들되 숨겨 두고, 지연 뒤 RevealResult로 보인다
	void ShowResult(const FWarriorStageResult& InResult);

	//숨겨 둔 결과 위젯을 보이고, UI 입력 모드면 포커스를 다시 준다
	void RevealResult();

	//스테이지 HUD (BP_StagePlayerController에서 WBP_StageHUD 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorStageHUDWidget> HUDWidgetClass;

	//결과 화면 (BP에서 WBP_StageResult 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSubclassOf<UWarriorStageResultWidget> ResultWidgetClass;

	//결과 화면의 "메인메뉴"로 이동할 레벨 (BP에서 L_Front 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	//실패 결과 화면을 보이기까지 기다리는 시간(초). 플레이어 사망 연출을 보여 주기 위함. 0이면 바로 보인다.
	//값은 사망 몽타주 길이에 맞춰 BP_StagePlayerController에서 정한다(현재 AM_Player_Death 5.0초 = 5).
	//사망 몽타주를 바꾸면 이 값도 함께 맞춘다. 사망 어빌리티 종료 방식은 위 [기술부채] 주석 참고
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Stage", meta = (ClampMin = "0.0", Units = "s"))
	float FailedResultDelay = 0.f;

private:
	TWeakObjectPtr<AWarriorStageGameState> BoundGameState;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWarriorStageResultWidget> ResultWidget;

	//결과 위젯을 숨기기 전의 표시 상태. RevealResult에서 되돌린다
	ESlateVisibility ResultVisibilityBeforeHide = ESlateVisibility::SelfHitTestInvisible;

	FTimerHandle ResultRevealTimer;

	//마지막으로 적용한 UI 입력 모드 값
	bool bAppliedUIInputMode = false;
};
