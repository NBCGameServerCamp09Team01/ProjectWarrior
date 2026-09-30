#pragma once

#include "CoreMinimal.h"
#include "WarriorStageTypes.generated.h"

//스테이지 진행 상태. 전환은 AWarriorStageGameMode
UENUM(BlueprintType)
enum class EWarriorStageState : uint8
{
	None,
	Initializing,	// 레벨 시작. 플레이어 스폰과 FlowManager 등록을 기다림
	Preparing,		// 준비 시간 (최초 쉬는 시간)
	InProgress,		// 웨이브 진행
	WaveCleared,	// 웨이브 종료 연출
	Resting,		// 쉬는 시간, 상점 가능
	StageCleared,	// 마지막(보스) 웨이브 클리어
	StageFailed		// 플레이어 사망
};

//상태 전환을 일으키는 사건. 스테이지의 상태가 변화해야할 경우 코드 외부에서 현재 Event를 발생시킨다.
UENUM(BlueprintType)
enum class EWarriorStageEvent : uint8
{
	StageReady,				// 초기화 완료 (GameMode 내부)
	StateTimerElapsed,		// 현재 상태의 타이머 종료 (GameMode 내부)
	AllEnemiesDead,			// 현재 웨이브 적 전멸 (FlowManager의 OnWaveCleared)
	WaveTimeLimitElapsed,	// 예약. MVP 미사용, 추후 제한시간 기믹에서 사용
	PlayerDied,				// 플레이어 사망
	SkipRest				// 쉬는 시간 건너뛰기 (선택)
};

//상태별로 허용 여부를 묻는 조작 종류
UENUM(BlueprintType)
enum class EWarriorStageAction : uint8
{
	Move,
	Combat,
	Interact,
	Shop
};

//한 상태에서 플레이어에게 허용되는 조작
USTRUCT(BlueprintType)
struct FWarriorStagePermission
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCanMove = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCanCombat = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCanInteract = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCanShop = false;

	//true면 UI 입력 모드와 마우스 커서 (결과 화면 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Stage")
	bool bUIInputMode = false;

	bool IsAllowed(EWarriorStageAction InAction) const
	{
		switch (InAction)
		{
			case EWarriorStageAction::Move:		return bCanMove;
			case EWarriorStageAction::Combat:	return bCanCombat;
			case EWarriorStageAction::Interact:	return bCanInteract;
			case EWarriorStageAction::Shop:		return bCanShop;
			default:							return false;
		}
	}
};

//스테이지 한 판의 결과. 결과 화면에 표시하고, 추후 웹 백엔드의 플레이 기록 저장에 쓴다.
//항목은 기획이 정해지면 늘어난다. EarnedGold·KillCount는 수집 경로가 정해질 때까지 0으로 둔다.
USTRUCT(BlueprintType)
struct FWarriorStageResult
{
	GENERATED_BODY()

	//한 판의 고유 번호. 같은 결과가 두 번 처리되지 않게 하는 기준(서버 연동 시 중복 지급 방지)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	FName StageId;

	//난이도별 적 스탯 차이는 아직 없다. 자리만 둔다.
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 Difficulty = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	bool bCleared = false;

	//마지막으로 시작한 웨이브. 1부터
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 ReachedWave = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 TotalWaveCount = 0;

	//한 판 시작(준비 시간 시작)부터 결과가 나올 때까지의 실제 시간(초). 시간 배율(인벤토리 휠의 슬로우)의 영향을 받지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	float PlayTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 EarnedGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Stage")
	int32 KillCount = 0;
};

// 델리게이트 매크로 선언
//FlowManager → GameMode: 현재 웨이브의 적이 전멸했고 대기 스폰도 없음. WaveNumber는 1부터
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorWaveCleared, int32, WaveNumber);
