// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorStagePlaySubsystem.generated.h"

struct FWarriorStageResult;

/**
 * 스테이지 플레이 서브시스템. 스테이지에 들어갈 때 서버에 플레이를 시작하고, 받은 stagePlayId를 보관한다.
 * 명세: 루트 docs/contracts/stage-play-api.md v1(P1 시작, P2 현황, P4 포기)
 *
 * - GameMode의 BeginRun이 StartStagePlay를 부른다. 게임은 시작 응답을 기다리지 않고 그대로 진행한다(준비 시간 안에 응답이 온다).
 * - GameMode의 FinishRun이 SubmitResult를 부른다. 서버가 검사·보상·저장을 하고 플레이를 CLEARED·FAILED로 끝낸다.
 * - 진행 중인 플레이가 남아 있으면(409 STAGE_PLAY_IN_PROGRESS, 이전 PIE를 중간에 끈 경우) 그 플레이를 포기하고 한 번 다시 시작한다.
 *   보관한 결과를 먼저 보내는 길(재전송)은 다음 작업이다.
 * - 요청은 UWarriorAuthSubsystem::SendAuthorized로 보낸다(토큰·401 처리는 Auth가 맡는다).
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStagePlaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorStagePlaySubsystem* Get(const UObject* WorldContextObject);

	//POST /accounts/me/stage-plays. 이전 플레이 ID는 지우고, 201이면 새 ID를 보관한다.
	//InStageId는 소문자로 바꿔 보낸다(예: stage.01.01)
	void StartStagePlay(FName InStageId);

	//서버가 발급한 지금 플레이 ID(소문자 하이픈 GUID). 시작 응답 전이나 시작 실패면 비어 있다
	const FString& GetCurrentStagePlayId() const { return CurrentStagePlayId; }

	//POST /accounts/me/stage-plays/{CurrentStagePlayId}/result. 플레이 ID가 없으면(시작 응답 전·시작 실패) 경고 로그만 남기고 보내지 않는다.
	//retryable true·연결 실패면 같은 requestId로 한 번 더 보낸다(서버는 같은 플레이의 결과를 한 번만 반영한다)
	void SubmitResult(const FWarriorStageResult& InResult);

private:
	//시작 요청을 보낸다. bInRecoverInProgress이면 409 STAGE_PLAY_IN_PROGRESS 때 포기 뒤 한 번 다시 시작한다(무한 반복 방지)
	void SendStart(const FString& InStageId, bool bInRecoverInProgress);

	void HandleStartResponse(int32 InStartSerial, const FString& InStageId, bool bInRecoverInProgress, int32 Status, const FString& Body);

	//GET /accounts/me/stage-plays/current → (있으면) 포기 → 다시 시작
	void RecoverInProgress(int32 InStartSerial, const FString& InStageId);

	//POST /accounts/me/stage-plays/{id}/abandon → 다시 시작
	void AbandonAndRestart(int32 InStartSerial, const FString& InStagePlayId, const FString& InStageId);

	//결과 본문을 보낸다. InAttempt는 1부터. 보내지 못하면 false
	bool SendResult(const FString& InStagePlayId, const FString& InRequestBody, int32 InAttempt);

	void HandleResultResponse(const FString& InStagePlayId, const FString& InRequestBody, int32 InAttempt, int32 Status, const FString& Body);

	//StartStagePlay를 부를 때마다 1씩 오른다. 늦게 온 지난 시작의 응답이 새 플레이 ID를 덮지 않게 비교한다
	bool IsCurrentStart(int32 InStartSerial) const { return InStartSerial == StartSerial; }

	FString CurrentStagePlayId;

	int32 StartSerial = 0;
};
