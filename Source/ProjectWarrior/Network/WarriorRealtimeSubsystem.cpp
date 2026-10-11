// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorRealtimeSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "IWebSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"

namespace WarriorRealtime
{
	//realtime-api.md "종료 코드"
	constexpr int32 CloseNormal = 1000;
	constexpr int32 CloseConnectionReplaced = 4000;
	constexpr int32 CloseSessionReplaced = 4001;
	constexpr int32 CloseSessionEnded = 4002;

	//realtime-api.md "메시지 종류"
	const TCHAR* const TypeSessionReplaced = TEXT("SESSION_REPLACED");
	const TCHAR* const TypeAccountChanged = TEXT("ACCOUNT_CHANGED");

	//기억해 둘 최근 메시지 번호 수
	constexpr int32 RecentMessageIdLimit = 32;
}

UWarriorRealtimeSubsystem* UWarriorRealtimeSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorRealtimeSubsystem>() : nullptr;
}

void UWarriorRealtimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//인증 서브시스템이 먼저 있어야 로그인·세션 종료 알림에 붙을 수 있다
	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleLoginCompleted);
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
}

void UWarriorRealtimeSubsystem::Deinitialize()
{
	StopReconnectTicker();
	CloseSocket(WarriorRealtime::CloseNormal, TEXT("shutdown"));

	if (UWarriorAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &ThisClass::HandleLoginCompleted);
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}

	Super::Deinitialize();
}

UWarriorAuthSubsystem* UWarriorRealtimeSubsystem::GetAuth() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UWarriorAuthSubsystem>() : nullptr;
}

//~ Begin 로그인 상태

void UWarriorRealtimeSubsystem::HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	if (!bSuccess)
	{
		return;
	}

	ReconnectAttempt = 0;
	RecentMessageIds.Reset();
	SetState(EWarriorRealtimeState::Connecting);
	Connect();
}

void UWarriorRealtimeSubsystem::HandleSessionEnded(EWarriorSessionEndReason Reason, const FText& Message)
{
	//로그아웃이면 서버도 1000으로 닫는다. 먼저 알림을 떼므로 그 닫힘이 다시 연결을 부르지 않는다
	StopReconnectTicker();
	CloseSocket(WarriorRealtime::CloseNormal, TEXT("session ended"));
	SetState(EWarriorRealtimeState::Disconnected);
}

//~ End 로그인 상태

//~ Begin 연결

void UWarriorRealtimeSubsystem::Connect()
{
	UWarriorAuthSubsystem* Auth = GetAuth();
	if (!Auth || !Auth->IsLoggedIn())
	{
		SetState(EWarriorRealtimeState::Disconnected);
		return;
	}

	CloseSocket(WarriorRealtime::CloseNormal, TEXT("reconnect"));

	ActiveSessionSerial = Auth->GetSessionSerial();
	Socket = Auth->CreateRealtimeSocket(RealtimePath);
	if (!Socket.IsValid())
	{
		ScheduleReconnect();
		return;
	}

	//소켓 알림은 게임 스레드에서 온다. CloseSocket이 RemoveAll(this)로 뗀다
	Socket->OnConnected().AddUObject(this, &ThisClass::HandleConnected);
	Socket->OnConnectionError().AddUObject(this, &ThisClass::HandleConnectionError);
	Socket->OnClosed().AddUObject(this, &ThisClass::HandleClosed);
	Socket->OnMessage().AddUObject(this, &ThisClass::HandleMessage);

	//주소에는 토큰이 없다(헤더에만 있음)
	UE_LOG(LogProjectWarrior, Log, TEXT("[Realtime] Connecting. path=%s attempt=%d"), *RealtimePath, ReconnectAttempt);
	Socket->Connect();
}

void UWarriorRealtimeSubsystem::CloseSocket(int32 InCode, const FString& InReason)
{
	if (!Socket.IsValid())
	{
		return;
	}

	Socket->OnConnected().RemoveAll(this);
	Socket->OnConnectionError().RemoveAll(this);
	Socket->OnClosed().RemoveAll(this);
	Socket->OnMessage().RemoveAll(this);

	if (Socket->IsConnected())
	{
		Socket->Close(InCode, InReason);
	}
	ReleaseSocketLater();

	if (UWarriorAuthSubsystem* Auth = GetAuth())
	{
		Auth->SetRealtimeConnected(false);
	}
}

void UWarriorRealtimeSubsystem::ReleaseSocketLater()
{
	//소켓 알림(OnClosed·OnMessage 등) 안에서 그 소켓을 바로 지우면 알림을 보내던 객체가 사라진다.
	//다음 틱까지 람다가 들고 있다가 놓는다(람다는 this를 잡지 않으므로 서브시스템이 먼저 사라져도 안전)
	if (!Socket.IsValid())
	{
		return;
	}

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[OldSocket = MoveTemp(Socket)](float) mutable
		{
			OldSocket.Reset();
			return false;
		}));
	Socket.Reset();
}

void UWarriorRealtimeSubsystem::ScheduleReconnect()
{
	UWarriorAuthSubsystem* Auth = GetAuth();
	if (!Auth || !Auth->IsLoggedIn())
	{
		SetState(EWarriorRealtimeState::Disconnected);
		return;
	}

	Auth->SetRealtimeConnected(false);

	//연결 실패의 이유(401인지)를 소켓은 알려 주지 않으므로 접속 점검으로 확인한다.
	//401이면 Auth가 로그인 상태를 끝내고 HandleSessionEnded가 예약을 지운다. 연결 실패·503이면 로그인은 그대로다
	Auth->CheckSessionNow();
	if (!Auth->IsLoggedIn())
	{
		return;
	}

	StopReconnectTicker();

	const float Delay = ReconnectDelaysSeconds.Num() > 0
		? ReconnectDelaysSeconds[FMath::Min(ReconnectAttempt, ReconnectDelaysSeconds.Num() - 1)]
		: 30.f;
	++ReconnectAttempt;

	ActiveSessionSerial = Auth->GetSessionSerial();
	SetState(EWarriorRealtimeState::Reconnecting);
	ReconnectTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleReconnectTick), FMath::Max(0.1f, Delay));

	UE_LOG(LogProjectWarrior, Log, TEXT("[Realtime] Reconnect scheduled in %.0fs (attempt %d)."), Delay, ReconnectAttempt);
}

bool UWarriorRealtimeSubsystem::HandleReconnectTick(float InDeltaTime)
{
	//한 번만 돈다(false를 돌려주면 티커가 스스로 빠진다)
	ReconnectTickerHandle.Reset();

	const UWarriorAuthSubsystem* Auth = GetAuth();
	if (!Auth || !Auth->IsLoggedIn() || Auth->GetSessionSerial() != ActiveSessionSerial)
	{
		return false;
	}

	Connect();
	return false;
}

void UWarriorRealtimeSubsystem::StopReconnectTicker()
{
	if (ReconnectTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
		ReconnectTickerHandle.Reset();
	}
}

//~ End 연결

//~ Begin 소켓 알림

void UWarriorRealtimeSubsystem::HandleConnected()
{
	ReconnectAttempt = 0;
	SetState(EWarriorRealtimeState::Connected);

	if (UWarriorAuthSubsystem* Auth = GetAuth())
	{
		Auth->SetRealtimeConnected(true);
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Realtime] Connected."));
}

void UWarriorRealtimeSubsystem::HandleConnectionError(const FString& InError)
{
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Realtime] Connection error: %s"), *InError);
	CloseSocket(WarriorRealtime::CloseNormal, TEXT("error"));
	ScheduleReconnect();
}

void UWarriorRealtimeSubsystem::HandleClosed(int32 InStatusCode, const FString& InReason, bool bWasClean)
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[Realtime] Closed. code=%d reason=%s clean=%s"),
		InStatusCode, *InReason, bWasClean ? TEXT("true") : TEXT("false"));

	//이 소켓은 이미 닫혔다. 알림을 떼고 버린다(Close를 다시 부르지 않음)
	if (Socket.IsValid())
	{
		Socket->OnConnected().RemoveAll(this);
		Socket->OnConnectionError().RemoveAll(this);
		Socket->OnClosed().RemoveAll(this);
		Socket->OnMessage().RemoveAll(this);
		ReleaseSocketLater();
	}

	UWarriorAuthSubsystem* Auth = GetAuth();
	if (Auth)
	{
		Auth->SetRealtimeConnected(false);
	}

	//realtime-api.md "종료 코드": 숫자로 판단한다
	switch (InStatusCode)
	{
	case WarriorRealtime::CloseSessionReplaced:
		//SESSION_REPLACED 메시지를 놓쳤어도 같은 처리
		if (Auth)
		{
			Auth->EndSessionByServer(EWarriorSessionEndReason::Replaced);
		}
		return;

	case WarriorRealtime::CloseSessionEnded:
		if (Auth)
		{
			Auth->EndSessionByServer(EWarriorSessionEndReason::Expired);
		}
		return;

	case WarriorRealtime::CloseNormal:
	case WarriorRealtime::CloseConnectionReplaced:
		//로그아웃이거나, 이 게임의 새 연결이 이전 연결을 밀어냄. 다시 연결하지 않는다
		SetState(EWarriorRealtimeState::Disconnected);
		return;

	default:
		//1001(서버 재시작), 4003(Pong 없음), 1006(비정상 끊김) 등: 로그인은 유지하고 다시 연결한다
		ScheduleReconnect();
		return;
	}
}

void UWarriorRealtimeSubsystem::HandleMessage(const FString& InMessage)
{
	TSharedPtr<FJsonObject> Envelope;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InMessage);
	if (!FJsonSerializer::Deserialize(Reader, Envelope) || !Envelope.IsValid())
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Realtime] Message could not be read. Ignored."));
		return;
	}

	//빠진 칸은 빈 문자열로 둔다(모르는 칸·빠진 칸에 깨지지 않게)
	FString Type;
	FString Id;
	Envelope->TryGetStringField(TEXT("type"), Type);
	Envelope->TryGetStringField(TEXT("id"), Id);
	if (!Id.IsEmpty() && IsDuplicateMessage(Id))
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Realtime] Duplicate message ignored. type=%s id=%s"), *Type, *Id);
		return;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Realtime] Message. type=%s id=%s"), *Type, *Id);

	if (Type == WarriorRealtime::TypeSessionReplaced)
	{
		//다른 곳 로그인: 기존 세션 종료 경로(타이틀 + 안내 팝업, 스테이지면 프론트로)를 그대로 탄다. 서버가 곧 4001로 닫는다
		if (UWarriorAuthSubsystem* Auth = GetAuth())
		{
			Auth->EndSessionByServer(EWarriorSessionEndReason::Replaced);
		}
		return;
	}

	if (Type == WarriorRealtime::TypeAccountChanged)
	{
		//예약 종류: v1에서 서버가 보내지 않는다(realtime-api.md "메시지 종류")
		UE_LOG(LogProjectWarrior, Log, TEXT("[Realtime] ACCOUNT_CHANGED is reserved in v1. Ignored."));
		return;
	}

	//모르는 종류는 무시한다(서버가 새 종류를 먼저 배포해도 깨지지 않게)
	UE_LOG(LogProjectWarrior, Verbose, TEXT("[Realtime] Unknown message type ignored: %s"), *Type);
}

//~ End 소켓 알림

bool UWarriorRealtimeSubsystem::IsDuplicateMessage(const FString& InId)
{
	if (RecentMessageIds.Contains(InId))
	{
		return true;
	}

	RecentMessageIds.Add(InId);
	if (RecentMessageIds.Num() > WarriorRealtime::RecentMessageIdLimit)
	{
		RecentMessageIds.RemoveAt(0);
	}
	return false;
}

void UWarriorRealtimeSubsystem::SetState(EWarriorRealtimeState InState)
{
	if (State == InState)
	{
		return;
	}

	State = InState;
	OnStateChanged.Broadcast(State);
}
