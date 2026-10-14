// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "ProjectWarrior/Network/WarriorStagePlayTypes.h"
#include "WarriorStagePlaySubsystem.generated.h"

struct FWarriorApiResult;
struct FWarriorPendingRequest;
struct FWarriorStageResult;
struct FWarriorStageResumeInfo;
struct FWarriorPlayCheckpoint;

//결과 제출이 끝났다(Submitting에서 다른 상태로). 결과 화면이 받아 보일지 정한다
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWarriorResultSubmitFinished, EWarriorResultSubmitState);

//판의 웨이브 구성 상태(M24, stage-play-api.md v1.3 "웨이브 구성")
UENUM()
enum class EWarriorWavePlanState : uint8
{
	None,			// StartStagePlay 전
	Waiting,		// 시작 요청(P1)을 보내고 응답을 기다림
	Ready,			// 서버 구성을 웨이브 데이터로 바꿈(GetWaveData)
	Failed,			// 이 판은 시작하지 않음(GetWavePlanError로 안내)
	NoServerPlay	// 서버 플레이 없음(로그인 없이 연 PIE, 서버 플레이 없는 디버그 이어하기). 개발 빌드면 로컬 웨이브로 대신할 수 있음
};

DECLARE_MULTICAST_DELEGATE(FOnWarriorWavePlanChanged);

/**
 * 스테이지 플레이 서브시스템. 스테이지에 들어갈 때 서버에 플레이를 시작하고, 받은 stagePlayId를 보관한다.
 * 명세: 루트 docs/contracts/stage-play-api.md v1(P1 시작, P2 현황, P4 포기), result-api.md v1(R1 결과 제출)
 *
 * - GameMode의 BeginRun이 StartStagePlay를 부른다. 게임은 시작 응답을 기다리지 않고 그대로 진행한다(준비 시간 안에 응답이 온다).
 * - GameMode의 FinishRun이 SubmitResult를 부른다. 서버가 검사·보상·저장을 하고 플레이를 CLEARED·FAILED로 끝낸다.
 *   결과 제출은 UWarriorApiSubsystem(S5)으로 보낸다: 자동 재시도 → "다시 시도 / 종료" → 계속 실패하면 디스크에 보관,
 *   다음 로그인 때 같은 requestId로 재전송한다. 서버가 거절하면(retryable false) 문구 표의 안내 팝업을 띄운다.
 * - 결과 제출의 상태(GetResultSubmitState)와 끝 알림(OnResultSubmitFinished)을 낸다. 결과 화면은 답이 올 때까지 숨어 있다가
 *   저장·거절이면 보이고, 연결되지 않아 보관 후 종료로 가면 보이지 않는다(AWarriorStagePlayerController).
 * - 결과가 저장되면(결과 응답, 판 시작 전 보관 결과 재전송) 응답의 계정 값을 ApplyAccountSnapshot으로 적용한다.
 *   보낼 때와 같은 세션·계정일 때만(M25, account-api.md 동기화 시점 T2·T5b, WarriorAccountSync.h).
 * - 진행 중인 플레이가 남아 있으면(409 STAGE_PLAY_IN_PROGRESS) 그 플레이의 보관 결과가 있으면 먼저 보내고,
 *   없으면 포기한 뒤 한 번 다시 시작한다(stage-play-api.md P1).
 * - 시작·포기·현황 요청은 지금처럼 UWarriorAuthSubsystem::SendAuthorized로 보낸다(한 번 보내고 끝, 토큰·401 처리는 Auth가 맡는다).
 * - 로그인 상태가 끝나면 플레이 ID를 비운다(다른 계정이 이어받지 않게).
 * - 이어하기(stage-play-api.md "이어하기(1차)"): 로그인 뒤 UWarriorPlayCheckpointSubsystem이 P2·P4를 부르고,
 *   이어하면 BeginRun이 이어하기 정보를 넘겨 P1 없이 같은 stagePlayId로 시작한다.
 * - 웨이브 구성(M24): 판의 웨이브는 서버가 시작 때 고정한 구성(waves)으로만 만든다. 보통 시작은 P1 201 응답, 이어하기는
 *   이어하기 정보의 WavePlan(로그인 때 받은 P2)을 ApplyWavePlan 한 입구로 넣는다. 몬스터 키는 DA_MonsterCatalog로 BP를 찾는다.
 *   구성을 쓸 수 없으면(P1 실패, waves 없음, 모르는 몬스터) 상태를 Failed로 두고 GameMode가 안내 후 메인메뉴로 간다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStagePlaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStagePlaySubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//진행 중 플레이 결과. InPlay는 InProgress일 때만 채워진다
	using FCurrentStagePlayCallback = TFunction<void(EWarriorCurrentStagePlay InResult, const FWarriorStagePlayDto& InPlay)>;

	//포기 결과. InStatus는 HTTP 상태(연결 실패는 0), InErrorCode는 오류 본문의 code
	using FAbandonCallback = TFunction<void(int32 InStatus, const FString& InErrorCode)>;

	//POST /accounts/me/stage-plays. 이전 플레이 ID는 지우고, 201이면 새 ID를 보관한다.
	//InStageId는 소문자로 바꿔 보낸다(예: stage.01.01)
	//InResume이 있으면(이어하기) 시작 요청을 보내지 않고 그 플레이 ID와 마감을 그대로 쓴다
	void StartStagePlay(FName InStageId, const FWarriorStageResumeInfo* InResume = nullptr);

	//GET /accounts/me/stage-plays/current. 보내지 못했으면(로그인 안 함) false이고 콜백은 오지 않는다
	bool RequestCurrentStagePlay(FCurrentStagePlayCallback InCallback);

	//POST /accounts/me/stage-plays/{id}/abandon. 보내지 못했으면 false이고 콜백은 오지 않는다
	bool AbandonStagePlay(const FString& InStagePlayId, FAbandonCallback InCallback);

	//체크포인트 저장 결과. InStatus는 HTTP 상태(연결 실패는 0), InErrorCode는 오류 본문의 code
	using FCheckpointSaveCallback = TFunction<void(int32 InStatus, const FString& InErrorCode)>;

	//PUT /accounts/me/stage-plays/{CurrentStagePlayId}/checkpoint(C1). 덮어쓴다. 재시도·보관은 하지 않는다(다음 웨이브 클리어 때 새로 올림).
	//플레이 ID가 없거나(시작 전·시작 실패·디버그) 보내지 못했으면 false이고 콜백은 오지 않는다
	bool SaveCheckpoint(const FWarriorPlayCheckpoint& InCheckpoint, FCheckpointSaveCallback InCallback);

	//체크포인트 받기 결과. InCheckpoint는 Found일 때만 채워진다
	using FCheckpointFetchCallback = TFunction<void(EWarriorCheckpointFetch InResult, const FWarriorPlayCheckpoint& InCheckpoint)>;

	//GET /accounts/me/stage-plays/{id}/checkpoint(C2). 보내지 못했으면 false이고 콜백은 오지 않는다
	bool RequestCheckpoint(const FString& InStagePlayId, FCheckpointFetchCallback InCallback);

	//지금 플레이를 포기하고 ID를 비운다. 판 도중 스스로 나가는 메뉴(일시정지 → 나가기)용. 앱 종료·세션 종료에는 부르지 않는다(다음 로그인 때 이어하기를 묻는다)
	void AbandonCurrentStagePlay();

	//서버가 발급한 지금 플레이 ID(소문자 하이픈 GUID). 시작 응답 전이나 시작 실패면 비어 있다
	const FString& GetCurrentStagePlayId() const { return CurrentStagePlayId; }

	//~ Begin 웨이브 구성(M24)
	//구성 상태가 바뀌었을 때. 이어하기면 StartStagePlay 안에서 바로 온다(GameMode는 그 전에 구독한다)
	FOnWarriorWavePlanChanged OnWavePlanChanged;

	EWarriorWavePlanState GetWavePlanState() const { return WavePlanState; }

	//Ready일 때 FlowManager에 넣을 웨이브 데이터(몬스터 BP를 찾아 둔 것)
	const TArray<FWarriorStageWaveData>& GetWaveData() const { return CurrentWaveData; }

	//지금 판의 서버 구성(웨이브 정보 버전 포함)
	const FWarriorStageWavePlan& GetWavePlan() const { return CurrentWavePlan; }

	//Failed일 때 안내할 오류. 문구는 오류 문구 표, 보이는 방법은 늘 팝업, 다음 행동은 늘 메인메뉴
	const FWarriorApiResult& GetWavePlanError() const { return WavePlanError; }

	//게임에서 정한 판 시작 실패(예: 구성 기다림 한도를 넘은 STAGE_WAVE_PLAN_TIMEOUT). 상태를 Failed로 바꾸고 알린다
	void FailWavePlan(const FString& InGameErrorCode);

	//판 시작 실패를 안내용 결과로 만든다(늘 팝업 + 메인메뉴). 자동화 시험에서도 쓴다
	static FWarriorApiResult MakeStageStartError(const FWarriorApiError& InError);
	//~ End 웨이브 구성

	//POST /accounts/me/stage-plays/{CurrentStagePlayId}/result. 플레이 ID가 없으면(시작 응답 전·시작 실패) 경고 로그만 남기고 보내지 않는다.
	//실패 처리(재시도·보관·안내)는 UWarriorApiSubsystem이 한다(서버는 같은 플레이의 결과를 한 번만 반영한다)
	//보내면 상태가 Submitting이 되고, 보내지 못하면 바로 NotSent로 끝난다(OnResultSubmitFinished)
	void SubmitResult(const FWarriorStageResult& InResult);

	//이번 판 결과 제출의 상태. StartStagePlay가 None으로 되돌린다
	EWarriorResultSubmitState GetResultSubmitState() const { return ResultSubmitState; }

	//결과 제출의 답을 기다리는 중인가
	bool IsSubmittingResult() const { return ResultSubmitState == EWarriorResultSubmitState::Submitting; }

	//결과 제출 응답(재시도·묻기를 거친 최종 결과)을 끝 상태로 바꾼다. Submitting·None·NotSent는 나오지 않는다
	static EWarriorResultSubmitState ClassifyResultResponse(const FWarriorApiResult& InResult);

	//결과 제출이 끝났을 때 한 번 온다(NotSent는 SubmitResult 안에서 바로 온다)
	FOnWarriorResultSubmitFinished OnResultSubmitFinished;

private:
	//결과 제출 상태를 끝 상태로 바꾸고 알린다
	void FinishResultSubmit(EWarriorResultSubmitState InState);

	//시작 요청을 보낸다. bInRecoverInProgress이면 409 STAGE_PLAY_IN_PROGRESS 때 포기 뒤 한 번 다시 시작한다(무한 반복 방지)
	void SendStart(const FString& InStageId, bool bInRecoverInProgress);

	void HandleStartResponse(int32 InStartSerial, const FString& InStageId, bool bInRecoverInProgress, int32 Status, const FString& Body);

	//GET /accounts/me/stage-plays/current → (보관 결과가 있으면) 결과 재전송 / (없으면) 포기 → 다시 시작.
	//로그인 때 이어하기를 물었으므로 이 길은 그 확인이 실패했거나 그 사이 다른 기기에서 새 판을 시작한 경우에만 탄다
	void RecoverInProgress(int32 InStartSerial, const FString& InStageId);

	//진행 중 플레이의 보관 결과를 먼저 보낸다. 받아들여지면 다시 시작, 거절되면 보관본을 지우고 포기 → 다시 시작
	void ResendStoredThenRestart(int32 InStartSerial, const FWarriorPendingRequest& InStored, const FString& InStagePlayId, const FString& InStageId);

	//POST /accounts/me/stage-plays/{id}/abandon → 다시 시작
	void AbandonAndRestart(int32 InStartSerial, const FString& InStagePlayId, const FString& InStageId);

	//결과 응답. 성공이면 보낼 때와 같은 세션·계정일 때만 응답 account를 ApplyAccountSnapshot으로 적용한다(M25, 동기화 시점 T2).
	//끝나면 결과 제출 상태를 바꿔 알린다(보낸 판이 지금 판일 때만)
	void HandleResultResult(const FString& InStagePlayId, int32 InSentStartSerial, int32 InSentSessionSerial, const FString& InSentAccountId,
		const FWarriorApiResult& InResult);

	//결과 응답 본문의 account를 사본에 적용한다. 보낼 때와 같은 세션·계정이 아니거나 account를 읽지 못하면 적용하지 않고 false
	bool ApplyResultAccount(const FString& InStagePlayId, int32 InSentSessionSerial, const FString& InSentAccountId,
		const FWarriorAccountSnapshotDto& InAccount);

	//로그인 상태가 끝났다(UWarriorAuthSubsystem::OnSessionEnded). 플레이 ID를 비운다
	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage);

	//~ Begin 웨이브 구성(M24)
	//이전 판 구성을 비운다(알리지 않음). StartStagePlay 첫머리·세션 끝에서 부른다
	void ClearWavePlan();

	//서버 구성을 넣는 한 입구(P1 201, 이어하기). 비었으면 STAGE_WAVE_PLAN_MISSING, 모르는 몬스터면 STAGE_WAVE_MONSTER_UNKNOWN
	void ApplyWavePlan(const FWarriorStageWavePlan& InPlan);

	void SetWavePlanState(EWarriorWavePlanState InState);

	//서버·연결 오류로 판을 시작하지 못함
	void FailWavePlanWithError(const FWarriorApiError& InError);

	EWarriorWavePlanState WavePlanState = EWarriorWavePlanState::None;
	FWarriorStageWavePlan CurrentWavePlan;
	TArray<FWarriorStageWaveData> CurrentWaveData;
	FWarriorApiResult WavePlanError;
	//~ End 웨이브 구성

	//StartStagePlay를 부를 때마다 1씩 오른다. 늦게 온 지난 시작의 응답이 새 플레이 ID를 덮지 않게 비교한다
	bool IsCurrentStart(int32 InStartSerial) const { return InStartSerial == StartSerial; }

	FString CurrentStagePlayId;

	//지금 플레이의 마감(시작 + 24시간). 결과 보관 유효 기간. 못 읽었으면 비어 있다(설정 기본값을 쓴다)
	FDateTime CurrentStagePlayExpiresAt;

	EWarriorResultSubmitState ResultSubmitState = EWarriorResultSubmitState::None;

	int32 StartSerial = 0;
};
