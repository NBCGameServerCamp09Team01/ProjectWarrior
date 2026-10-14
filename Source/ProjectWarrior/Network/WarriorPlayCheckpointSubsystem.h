// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorStagePlayTypes.h"
#include "WarriorPlayCheckpointSubsystem.generated.h"

//로그인 뒤 진행 중 플레이 확인 상태
UENUM(BlueprintType)
enum class EWarriorPlayCheckpointState : uint8
{
	Idle,			// 로그인 전
	WaitingResend,	// 로그인함. 보관 요청 재전송이 끝나기를 기다림
	Checking,		// 진행 중 플레이를 묻는 중(P2)
	None,			// 이어할 플레이 없음 → 메인메뉴
	Resumable,		// 이어할 수 있는 플레이 있음 → 이어하기 팝업
	Abandoning,		// 포기 요청 중(P4)
	Blocked			// 묻지 않음(그 플레이의 보관 결과가 남음, 확인 실패, 시간 초과) → 메인메뉴
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorPlayCheckpointChanged, EWarriorPlayCheckpointState, State);

/**
 * 플레이 체크포인트(M22): 로그인 뒤 서버에 진행 중인 플레이가 남아 있으면 이어 할지 묻게 한다.
 * 명세: 루트 docs/contracts/stage-play-api.md "이어하기"·"C1·C2. 체크포인트", 흐름도 docs/flows/play-checkpoint.md
 *
 * - 로그인 → 보관 요청 재전송(UWarriorApiSubsystem)이 끝나면 → P2 → 진행 중이면 C2. 결과를 OnStateChanged로 알린다.
 * - 이어하기: 같은 stagePlayId로 그 스테이지를 연다(Resume → 스테이지 레벨의 BeginRun).
 *   체크포인트가 있고 버전이 같으면 그 판 안 값으로 클리어한 웨이브 다음의 쉬는 시간부터, 없으면 처음(1웨이브)부터.
 * - 포기: P4.
 * - 체크포인트 올리기(C1)는 스테이지 GameMode가 웨이브를 클리어할 때 한다(UploadWaveCheckpoint).
 */
UCLASS(Config = Game)
class PROJECTWARRIOR_API UWarriorPlayCheckpointSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorPlayCheckpointSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	UFUNCTION(BlueprintPure, Category = "Warrior|PlayCheckpoint")
	EWarriorPlayCheckpointState GetState() const { return State; }

	//확인이 아직 끝나지 않았다(메인메뉴 입력을 막는다)
	UFUNCTION(BlueprintPure, Category = "Warrior|PlayCheckpoint")
	bool IsChecking() const { return State == EWarriorPlayCheckpointState::WaitingResend || State == EWarriorPlayCheckpointState::Checking; }

	//이어할 플레이(Resumable일 때만 의미가 있다)
	const FWarriorStagePlayDto& GetResumablePlay() const { return ResumablePlay; }

	//이어할 체크포인트(Resumable일 때, 없으면 IsSet() false → 처음부터)
	const FWarriorPlayCheckpoint& GetResumableCheckpoint() const { return ResumableCheckpoint; }

	//이어하면 시작할 웨이브. 체크포인트가 있으면 클리어한 웨이브 + 1, 없으면 1(처음부터)
	int32 GetResumeFromWave() const { return ResumableCheckpoint.IsSet() ? ResumableCheckpoint.ClearedWave + 1 : 1; }

	//이 빌드에서 이어할 수 있는가: 스테이지 목록(카탈로그)에 그 스테이지가 있고, 웨이브 구성(M24)이 있으며 몬스터 키가 모두 몬스터 카탈로그에 있다
	bool CanResumeInThisBuild() const;

	//이어하기를 고른다. 이어하기 정보를 실행 정보에 넣는다. 레벨은 부른 쪽이 연다(StartStage). 이어할 수 없으면 false
	bool Resume();

	//포기를 고른다(P4). InOnDone(bEnded): 서버에서 플레이가 끝났으면 true(200·404·409), 연결 실패·5xx면 false
	void Abandon(TFunction<void(bool bEnded)> InOnDone);

	UPROPERTY(BlueprintAssignable, Category = "Warrior|PlayCheckpoint")
	FOnWarriorPlayCheckpointChanged OnStateChanged;

private:
	UFUNCTION()
	void HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

	UFUNCTION()
	void HandleResendFinished(int32 Sent, int32 Rejected);

	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage);

	//P2를 보낸다
	void StartCheck();

	void HandleCurrentStagePlay(int32 InCheckSerial, EWarriorCurrentStagePlay InResult, const FWarriorStagePlayDto& InPlay);

	//진행 중 플레이의 체크포인트(C2)를 받았을 때. 없거나 버전이 다르거나 실패하면 처음부터 이어 간다
	void HandleCheckpointFetched(int32 InCheckSerial, EWarriorCheckpointFetch InResult, const FWarriorPlayCheckpoint& InCheckpoint);

	//확인을 끝낸다(타이머를 끄고 상태를 바꾼다)
	void FinishCheck(EWarriorPlayCheckpointState InState);

	//확인 시간 상한이 지났다
	void HandleCheckTimeout();

	void SetState(EWarriorPlayCheckpointState InState);

	void ClearCheckTimer();

	//확인을 기다리는 시간 상한(초). 넘으면 묻지 않고 메인메뉴(Blocked). DefaultGame.ini [/Script/ProjectWarrior.WarriorPlayCheckpointSubsystem]
	UPROPERTY(Config)
	float CheckTimeoutSeconds = 10.f;

	EWarriorPlayCheckpointState State = EWarriorPlayCheckpointState::Idle;

	FWarriorStagePlayDto ResumablePlay;

	//ResumablePlay의 체크포인트(M22 최종). 비어 있으면 처음부터
	FWarriorPlayCheckpoint ResumableCheckpoint;

	//로그인·세션 종료마다 1씩 오른다. 늦게 온 지난 확인의 응답을 버린다
	int32 CheckSerial = 0;

	//보관 요청 재전송이 끝난 로그인(UWarriorAuthSubsystem의 세션 번호). 재전송 끝 알림이 로그인 알림보다 먼저 와도 놓치지 않게 기억한다
	int32 ResendFinishedSessionSerial = INDEX_NONE;

	FTimerHandle CheckTimerHandle;
};
