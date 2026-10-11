// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "WarriorRealtimeSubsystem.generated.h"

class IWebSocket;
class UWarriorAuthSubsystem;

//실시간 연결 상태. 화면은 Reconnecting일 때 "재연결 중"을 띄운다
UENUM(BlueprintType)
enum class EWarriorRealtimeState : uint8
{
	Disconnected,	// 로그인하지 않았거나 다시 연결하지 않기로 함
	Connecting,		// 로그인 직후 첫 연결 중
	Connected,
	Reconnecting	// 끊겨서 다시 연결하는 중(로그인은 유지)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorRealtimeStateChanged, EWarriorRealtimeState, State);

/**
 * 실시간 연결(S7). 명세: 루트 docs/contracts/realtime-api.md v1.1
 *
 * - 로그인에 성공하면 /realtime에 WebSocket으로 붙는다(토큰은 UWarriorAuthSubsystem이 헤더에 싣는다).
 * - GameInstance 서브시스템이라 레벨(프론트 ↔ 스테이지)을 옮겨도 연결이 유지된다.
 * - 서버 Ping에는 엔진이 Pong으로 답하고, 서버는 그때마다 세션을 늘린다.
 * - 서버 메시지는 봉투(type, id, sentAt, data)로 온다. 같은 id는 한 번만, 모르는 type은 무시한다.
 *   SESSION_REPLACED(다른 곳 로그인) → 로그인 상태를 끝낸다(화면 처리는 기존 OnSessionEnded 경로).
 * - 끊기면 로그인은 유지한 채 1·2·4·8·16·30초 간격으로 다시 연결한다. 스테이지 플레이는 끊지 않는다.
 *   로그인이 끝나는 것은 서버가 401(접속 점검)이나 종료 코드 4001·4002를 줄 때뿐이다.
 * - UE IWebSocket은 핸드셰이크 실패의 상태 코드를 주지 않으므로, 연결에 실패하면 접속 점검(A4)으로 세션이 살아 있는지 확인한다.
 */
UCLASS(Config = Game)
class PROJECTWARRIOR_API UWarriorRealtimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorRealtimeSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	UFUNCTION(BlueprintPure, Category = "Warrior|Realtime")
	EWarriorRealtimeState GetState() const { return State; }

	//상태가 바뀔 때. 프론트 컨트롤러가 "재연결 중" 표시에 쓴다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Realtime")
	FOnWarriorRealtimeStateChanged OnStateChanged;

protected:
	//연결 경로. 주소는 인증 서브시스템의 BaseUrl을 ws로 바꿔 붙인다
	UPROPERTY(Config)
	FString RealtimePath = TEXT("/realtime");

	//다시 연결 간격(초). 마지막 값을 계속 쓴다(realtime-api.md "다시 연결")
	UPROPERTY(Config)
	TArray<float> ReconnectDelaysSeconds = { 1.f, 2.f, 4.f, 8.f, 16.f, 30.f };

private:
	//로그인 결과(UWarriorAuthSubsystem::OnLoginCompleted). 성공이면 연결한다
	UFUNCTION()
	void HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

	//로그인 상태가 끝났다(UWarriorAuthSubsystem::OnSessionEnded). 닫고 다시 연결하지 않는다
	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason Reason, const FText& Message);

	//새 소켓을 만들어 붙는다. 이전 소켓은 알림을 떼고 닫는다
	void Connect();

	//소켓 알림을 떼고 닫는다. 떼는 것이 먼저라, 닫힘 알림이 다시 연결을 부르지 않는다
	void CloseSocket(int32 InCode, const FString& InReason);

	//Socket을 비우되, 객체는 다음 틱에 놓는다(소켓 알림 안에서 불려도 안전하게)
	void ReleaseSocketLater();

	//다시 연결을 예약한다. 접속 점검으로 세션이 살아 있는지도 확인한다(401이면 Auth가 로그인 상태를 끝내고, 예약은 버려진다)
	void ScheduleReconnect();

	//다시 연결 티커(한 번). 예약한 뒤 로그아웃·재로그인했다면 아무것도 하지 않는다
	bool HandleReconnectTick(float InDeltaTime);

	void StopReconnectTicker();

	//~ Begin 소켓 알림(게임 스레드)
	void HandleConnected();
	void HandleConnectionError(const FString& InError);
	void HandleClosed(int32 InStatusCode, const FString& InReason, bool bWasClean);
	void HandleMessage(const FString& InMessage);
	//~ End 소켓 알림

	//이미 처리한 메시지 번호면 true. 아니면 기억하고 false
	bool IsDuplicateMessage(const FString& InId);

	void SetState(EWarriorRealtimeState InState);

	UWarriorAuthSubsystem* GetAuth() const;

	TSharedPtr<IWebSocket> Socket;

	FTSTicker::FDelegateHandle ReconnectTickerHandle;

	//다음 다시 연결이 ReconnectDelaysSeconds의 몇 번째 값을 쓸지. 연결되면 0
	int32 ReconnectAttempt = 0;

	//연결·다시 연결을 시작한 세션 번호(UWarriorAuthSubsystem::GetSessionSerial). 다르면 지난 세션의 일이다
	int32 ActiveSessionSerial = 0;

	//최근에 처리한 메시지 번호(재연결 때 같은 알림이 다시 와도 한 번만 처리)
	TArray<FString> RecentMessageIds;

	EWarriorRealtimeState State = EWarriorRealtimeState::Disconnected;
};
