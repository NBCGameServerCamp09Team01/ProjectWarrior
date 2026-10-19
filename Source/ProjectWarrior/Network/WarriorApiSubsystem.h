// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "ProjectWarrior/Network/WarriorPendingRequestStore.h"
#include "WarriorApiSubsystem.generated.h"

//팝업을 띄워 달라는 알림. 팝업 담당(UWarriorErrorPresenterSubsystem)이 받는다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorErrorNoticeRequested, const FWarriorErrorNotice&, Notice);

//로그인 뒤 보관 요청 재전송이 끝났다. Sent는 서버가 받은 수, Rejected는 서버가 거절했거나 보낼 수 없어 지운 수
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorPendingResendFinished, int32, Sent, int32, Rejected);

/**
 * 서버 요청 실패 처리 한 곳(S5 UF-1). 명세: 루트 docs/contracts/README.md 공통 규칙·재전송 규칙
 *
 * - Send(요청, 결과 콜백): 요청 하나 → 결과 하나. 토큰·BaseUrl·제한 시간은 UWarriorAuthSubsystem이 맡는다.
 * - 응답 없음·5xx·retryable true → UWarriorApiSettings의 간격으로 자동 재시도.
 *   다 실패하면 bAskOnFail인 요청은 "다시 시도 / 종료"(보관했으면 "보관 후 종료")를 한 번 묻고, 종료(또는 다시 실패)면 보관한 뒤 "게임을 종료합니다" 안내 후 종료한다.
 * - 401 → 보관 대상이면 먼저 보관하고 인증 서브시스템이 로그인 상태를 끝낸다(보낼 때와 같은 세션일 때만).
 * - retryable false → 다시 보내지 않는다. 화면에 보일지는 호출한 쪽이 정한다(PresentError).
 * - 로그인에 성공하면 그 계정의 보관 요청을 같은 requestId·같은 본문으로 다시 보낸다(UWarriorPendingRequestStore).
 * - 타이틀에서 서버 상태를 확인하고, 실패하면 안내 후 종료한다(CheckServerOnTitle).
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorApiSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//요청을 보낸다. 결과(성공·최종 실패)가 나오면 OnDone을 한 번 부른다.
	//인증 요청인데 로그인하지 않았거나 요청을 시작하지 못하면 false(OnDone은 부르지 않는다).
	//OnDone이 부른 쪽 객체를 잡으면 약한 참조로 잡는다(응답 전에 사라질 수 있다)
	bool Send(const FWarriorApiRequest& InRequest, TFunction<void(const FWarriorApiResult&)>&& OnDone);

	//실패 결과를 문구 표의 방법대로 보인다. Popup이면 팝업 알림을 낸다. 나머지 방법(화면 안 문구 등)은 화면이 직접 한다
	void PresentError(const FWarriorApiResult& InResult);

	//안내 팝업 하나를 띄워 달라고 알린다(오류가 아닌 안내 포함)
	void RequestNotice(const FWarriorErrorNotice& InNotice);

	//"다시 시도 / 종료" 팝업의 답. bRetry면 한 번 더 보내고, 아니면 보관 후 종료 안내
	UFUNCTION(BlueprintCallable, Category = "Warrior|Api")
	void AnswerRetryPrompt(bool bRetry);

	//타이틀에서 한 번 서버 상태를 확인한다(실행마다 한 번). 실패하면 안내 후 종료 팝업을 낸다
	void CheckServerOnTitle();

	//보관 요청 재전송 중인가
	UFUNCTION(BlueprintPure, Category = "Warrior|Api")
	bool IsResendingPending() const { return bResendingPending; }

	//실패 응답 본문을 읽는다. 연결 실패(0)는 NETWORK_ERROR, 본문을 못 읽으면 HTTP_<상태>. HttpStatus를 채운다
	static FWarriorApiError ParseError(int32 InStatus, const FString& InBody);

	//다시 보내도 되는 실패인가: 응답 없음, retryable true, 본문을 못 읽은 5xx
	static bool IsRetryable(const FWarriorApiError& InError);

	//팝업 담당이 받는다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Api")
	FOnWarriorErrorNoticeRequested OnNoticeRequested;

	//로그인 뒤 보관 요청 재전송이 끝났을 때. 보낼 것이 없을 때도 (0, 0)으로 온다. 재전송 도중 로그인 상태가 끝나면 오지 않는다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Api")
	FOnWarriorPendingResendFinished OnPendingResendFinished;

private:
	//보내는 중인 요청 하나
	struct FCall
	{
		int32 Id = 0;
		FWarriorApiRequest Request;
		TFunction<void(const FWarriorApiResult&)> OnDone;
		//보낸 횟수
		int32 Attempts = 0;
		//이번 차례의 자동 재시도 횟수("다시 시도"를 누르면 0부터 다시)
		int32 AutoRetries = 0;
		//"다시 시도 / 종료"를 이미 물었는가(한 번만 묻는다)
		bool bAsked = false;
		//이미 보관했는가(묻기 전·401에서 보관). 성공하면 보관본을 지운다
		bool bStored = false;
		//마지막 실패 결과("종료"를 고르면 이 결과로 끝낸다)
		FWarriorApiResult LastResult;
		//보낼 때의 세션 번호와 계정(401·보관에 씀). 계정 ID는 세션이 끝나도 보관할 수 있게 미리 잡는다
		int32 SessionSerial = 0;
		FString AccountId;
		FTSTicker::FDelegateHandle RetryTicker;
	};

	//보낸다. 시작하지 못하면 false
	bool Dispatch(int32 InCallId);
	void HandleResponse(int32 InCallId, int32 InStatus, const FString& InBody);

	//자동 재시도를 다 썼다. 묻거나 최종 실패로 끝낸다
	void HandleRetriesExhausted(int32 InCallId, const FWarriorApiResult& InResult);

	//최종 결과를 알리고 목록에서 뺀다. bStoreOnFail이면 실패일 때 보관한다
	void Finish(int32 InCallId, FWarriorApiResult InResult, bool bStoreOnFail);

	//보관 대상이면 디스크에 보관한다. 보관했으면 true
	bool StoreCall(const FCall& InCall);

	//보관 후 "게임을 종료합니다" 안내를 낸다
	void RequestQuitNotice(const FWarriorApiResult& InResult, bool bStored);

	//~ Begin 보관 요청 재전송
	UFUNCTION()
	void HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

	void ResendPending();
	void ResendNext();
	void FinishResend();
	//~ End 보관 요청 재전송

	TMap<int32, FCall> Calls;
	int32 NextCallId = 1;

	//"다시 시도 / 종료" 답을 기다리는 요청. 0이면 없음. 한 번에 하나만 묻는다
	int32 PromptCallId = 0;

	//재전송 상태
	bool bResendingPending = false;
	FString ResendAccountId;
	TArray<FWarriorPendingRequest> ResendQueue;
	int32 ResendSent = 0;
	int32 ResendRejected = 0;
	TArray<FString> ResendRejectReasons;
	int32 ResendSessionSerial = 0;

	bool bServerChecked = false;
};
