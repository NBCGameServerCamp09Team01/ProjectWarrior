// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorAuthTypes.h"
#include "WarriorAuthSubsystem.generated.h"

class IWebSocket;

//요청 결과. 실패하면 ErrorCode에 서버 오류 코드(예: AUTH_INVALID_CREDENTIALS), Message에 화면에 보여 줄 문장이 들어온다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWarriorAuthRequestCompleted, bool, bSuccess, const FString&, ErrorCode, const FText&, Message);

//로그인 상태가 끝났다. Message는 안내 팝업 문구(LoggedOut이면 비어 있다)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorSessionEnded, EWarriorSessionEndReason, Reason, const FText&, Message);

/**
 * 인증 서브시스템. 회원가입·로그인을 요청하고 로그인 상태(계정, 액세스 토큰)를 보관한다.
 * 명세: 루트 docs/contracts/auth-api.md
 *
 * - 로그인·회원가입 화면은 Request* 를 호출하고 OnLoginCompleted·OnSignupCompleted로 결과를 받는다.
 * - 프론트 컨트롤러는 IsLoggedIn으로 메인메뉴에 들어갈 수 있는지 판단한다.
 * - 입력 규칙(길이·문자)은 Validate* 에 모아 두고, 화면이 요청 전에 먼저 확인한다(서버도 다시 검증한다).
 * - 로그인 상태가 끝나면(로그아웃·서버 401·실시간 연결의 다른 곳 로그인) 모두 EndSession 한곳을 거쳐 OnSessionEnded로 알린다.
 *   레벨 이동 중에 알림을 놓친 화면은 ConsumePendingSessionEnd로 꺼낸다.
 * - 로그인해 있는 동안 HeartbeatIntervalSeconds마다 접속 점검(A4)을 보내 서버 세션(10분)을 늘리고 401을 알아챈다.
 *   연결 실패·5xx가 이어져도 로그인은 끝내지 않는다(realtime-api.md v1.1 "다시 연결". 끝내는 것은 서버 401뿐).
 * - 실시간 연결(UWarriorRealtimeSubsystem)이 붙어 있는 동안은 그 연결의 Ping/Pong이 세션을 늘린다.
 *   bPauseHeartbeatWhileRealtimeConnected가 켜져 있으면 그동안 접속 점검을 멈춘다.
 *
 * 회원가입(POST {BaseUrl}/auth/signup)과 로그인(POST {BaseUrl}/auth/login)은 웹서버에 실제로 요청한다.
 * 서버 주소는 DefaultGame.ini의 [/Script/ProjectWarrior.WarriorAuthSubsystem] BaseUrl로 바꿀 수 있다.
 * 액세스 토큰과 비밀번호는 어떤 로그에도 남기지 않는다.
 */
UCLASS(Config = Game)
class PROJECTWARRIOR_API UWarriorAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorAuthSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//POST {BaseUrl}/auth/signup. 결과는 OnSignupCompleted로 알린다.
	//가입만 하고 로그인은 하지 않는다(토큰을 주지 않으므로 이어서 로그인한다). 성공하면 가입했다는 것만 기억해 로그인 화면이 안내한다(아이디는 기억하지 않는다)
	//InEmail은 선택이다. 비어 있으면 JSON에서 칸을 빼고 보낸다(명세: 비워 두거나 null)
	//비밀번호 확인 칸은 화면에서만 비교하고 여기로 넘기지 않는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void RequestSignup(const FString& InLoginId, const FString& InPassword, const FString& InNickname, const FString& InEmail);

	//POST {BaseUrl}/auth/login. 결과는 OnLoginCompleted로 알린다.
	//성공하면 토큰을 보관하고 메인화면 값을 계정 서브시스템에 적용(ApplyServerSnapshot)한 뒤 성공을 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void RequestLogin(const FString& InLoginId, const FString& InPassword);

	//POST {BaseUrl}/auth/logout. 요청을 보낸 뒤 응답을 기다리지 않고 바로 로그인 상태를 끝낸다(OnSessionEnded, LoggedOut).
	//명세: 응답을 받든 못 받든 토큰을 지우고 타이틀로 간다. 서버에 닿지 못해도 서버 세션은 수명이 지나면 사라진다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void RequestLogout();

	//인증이 필요한 요청을 다른 서브시스템이 보낼 때 쓴다(예: 스테이지 플레이·결과). InVerb는 GET·POST.
	//BaseUrl·토큰·제한 시간은 이 서브시스템 값을 그대로 쓰고, 토큰은 밖으로 내보내지 않는다.
	//로그인하지 않았거나 요청을 시작하지 못하면 보내지 않고 false(OnDone은 부르지 않는다).
	//401이면 보낼 때와 같은 세션일 때만 먼저 로그인 상태를 끝내고(HandleAuthFailure → OnSessionEnded), 그다음 OnDone을 부른다.
	//OnDone(Status, Body)의 Status 0은 연결 실패. OnDone이 부른 쪽 객체를 잡으면 약한 참조로 잡는다(응답 전에 사라질 수 있다)
	bool SendAuthorized(const FString& InVerb, const FString& InPath, const FString& InJsonBody, TFunction<void(int32 /*Status*/, const FString& /*Body*/)>&& OnDone);

	UFUNCTION(BlueprintPure, Category = "Warrior|Auth")
	bool IsLoggedIn() const { return bLoggedIn; }

	//요청을 보내고 결과를 기다리는 중인가. 그동안 화면은 버튼을 막는다
	UFUNCTION(BlueprintPure, Category = "Warrior|Auth")
	bool IsRequestInFlight() const { return bRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Auth")
	const FWarriorAuthAccount& GetAccount() const { return Account; }

	//방금 회원가입에 성공했는지 한 번 꺼낸다. 로그인 화면이 "가입이 완료되었습니다" 안내를 띄울 때 쓴다
	bool ConsumeRecentSignup();

	//마지막 회원가입 실패의 칸별 오류. 키는 서버 칸 이름(loginId, nickname, email, password), 값은 화면 문구.
	//400 VALIDATION_FAILED의 errors[]와 409 중복(아이디·닉네임)을 담는다. 회원가입 화면이 해당 칸 아래에 표시한다
	const TMap<FString, FText>& GetLastSignupFieldErrors() const { return LastSignupFieldErrors; }

	//마지막 로그인 실패가 429 AUTH_LOGIN_LOCKED였다면 남은 잠김 초(retryAfterSeconds). 아니면 0.
	//로그인 화면이 그동안 로그인 버튼을 막는 데 쓴다
	int32 GetLoginRetryAfterSeconds() const { return LastLoginRetryAfterSeconds; }

	//~ Begin 입력 규칙. 문제가 없으면 빈 FText, 있으면 화면에 보여 줄 문장을 돌려준다
	//auth-api.md v1 A1 규칙. 아이디·닉네임 중복은 서버가 판정한다(409)
	//아이디: 영문 대소문자·숫자 4~20자, 대소문자 구분
	static FText ValidateLoginId(const FString& InLoginId);
	//비밀번호: 8자 이상, UTF-8 72바이트 이하(한글만이면 24자)
	static FText ValidatePassword(const FString& InPassword);
	//닉네임: 한글·영문·숫자 2~20자, 가운데 공백 가능, 앞뒤 공백 불가
	static FText ValidateNickname(const FString& InNickname);
	//이메일: 비어 있으면 통과(선택 입력, 보낼 때 칸을 뺀다). 값이 있으면 254자 이하, 이름@도메인.최상위 모양만 본다
	static FText ValidateEmail(const FString& InEmail);
	//~ End 입력 규칙

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Auth")
	FOnWarriorAuthRequestCompleted OnSignupCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Auth")
	FOnWarriorAuthRequestCompleted OnLoginCompleted;

	//로그인 상태가 끝났을 때. 프론트·스테이지 컨트롤러가 받아 화면을 옮긴다
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Auth")
	FOnWarriorSessionEnded OnSessionEnded;

	//마지막으로 로그인 상태가 끝난 이유와 안내 문구를 한 번 꺼낸다. 없으면 false.
	//스테이지에서 끊겨 프론트 레벨로 돌아온 경우처럼, 알림 뒤에 새로 만들어진 화면이 쓴다
	bool ConsumePendingSessionEnd(EWarriorSessionEndReason& OutReason, FText& OutMessage);

	//~ Begin 실시간 연결(UWarriorRealtimeSubsystem)이 쓰는 함수. 토큰은 밖으로 내보내지 않는다
	//{BaseUrl}{InPath}(http → ws, https → wss)로 Authorization 헤더를 붙인 WebSocket을 만든다(연결은 부른 쪽이 Connect).
	//로그인하지 않았으면 nullptr
	TSharedPtr<IWebSocket> CreateRealtimeSocket(const FString& InPath) const;

	//서버가 실시간 연결로 알린 이유(다른 곳 로그인, 세션 끝남)로 로그인 상태를 끝낸다. 이미 끝났으면 아무것도 하지 않는다
	void EndSessionByServer(EWarriorSessionEndReason InReason);

	//지금 바로 접속 점검(A4)을 한 번 보낸다. 실시간 연결에 실패했을 때 세션이 살아 있는지(401인지) 확인하는 데 쓴다.
	//401이면 평소처럼 로그인 상태를 끝내고(OnSessionEnded), 아니면 아무것도 하지 않는다
	void CheckSessionNow();

	//실시간 연결이 붙었는지 알린다. bPauseHeartbeatWhileRealtimeConnected가 켜져 있으면 붙은 동안 접속 점검을 멈추고 끊기면 다시 돌린다
	void SetRealtimeConnected(bool bConnected);

	//로그인할 때마다 바뀌는 번호. 실시간 연결이 늦게 온 지난 세션의 일을 버릴 때 비교한다
	int32 GetSessionSerial() const { return SessionSerial; }
	//~ End 실시간 연결

protected:
	//웹서버 주소 (끝에 / 없이). 경로는 /auth/login처럼 붙인다
	UPROPERTY(Config)
	FString BaseUrl = TEXT("http://localhost:8080");

	//요청 제한 시간(초). 넘으면 연결 실패(NETWORK_ERROR)로 처리한다
	UPROPERTY(Config)
	float RequestTimeoutSeconds = 10.f;

	//접속 점검 간격(초). 세션 수명(서버 pw01.auth.session.ttl, 10분)보다 충분히 짧게 둔다
	UPROPERTY(Config)
	float HeartbeatIntervalSeconds = 60.f;

	//실시간 연결이 붙어 있는 동안 접속 점검을 멈출지. 켜져 있으면 연결 중에는 서버 Ping에 엔진이 자동으로 보내는 Pong이 세션을 늘리고,
	//접속 점검은 연결이 없는 동안(처음 연결 전, 다시 연결하는 중)에만 돈다. Windows UE는 libwebsockets를 쓰고, Ping에 Pong을 자동으로 보낸다
	//(서버 로그 [Realtime] pong received로 확인. 서버 LOGGING_LEVEL_COM_PW01_WEBSERVER_REALTIME=DEBUG).
	//끄면 연결 중에도 점검을 계속 보낸다(세션은 어느 쪽으로든 늘어난다. 문제를 가릴 때만 끈다)
	UPROPERTY(Config)
	bool bPauseHeartbeatWhileRealtimeConnected = true;

private:
	//POST {BaseUrl}{InPath}를 보낸다. 응답(또는 연결 실패)이 오면 OnDone(Status, Body)를 부른다. Status 0은 연결 실패.
	//InJsonBody가 비어 있으면 본문 없이 보낸다(접속 점검·로그아웃).
	//bWithAuth이면 Authorization: Bearer <AccessToken>을 붙인다. 토큰이 없으면 보내지 않고 false.
	//요청을 시작하지 못하면 false. 본문(비밀번호)과 토큰은 로그에 남기지 않는다
	bool SendPost(const FString& InPath, const FString& InJsonBody, bool bWithAuth, TFunction<void(int32 /*Status*/, const FString& /*Body*/)>&& OnDone);

	//SendPost·SendAuthorized가 같이 쓰는 보내기. InVerb {BaseUrl}{InPath}. 나머지 규칙은 SendPost와 같다
	bool SendRequest(const FString& InVerb, const FString& InPath, const FString& InJsonBody, bool bWithAuth, TFunction<void(int32 /*Status*/, const FString& /*Body*/)>&& OnDone);

	//실패 응답 본문을 읽는다. 연결 실패(Status 0)는 NETWORK_ERROR, 본문을 못 읽으면 HTTP_<상태>로 채운다
	static FWarriorApiError ParseApiError(int32 InStatus, const FString& InBody);

	//회원가입·로그인이 같이 쓰는 문구(서버 장애, 연결 실패). 해당하지 않으면 빈 FText
	static FText CommonErrorToText(const FString& InCode, int32 InStatus);

	//회원가입 응답 처리. 201이면 성공
	void HandleSignupResponse(int32 Status, const FString& Body);

	//오류 코드로 회원가입 화면 문구를 고른다. VALIDATION_FAILED는 칸별 오류(errors[].message)를 보여 준다
	static FText SignupErrorToText(const FWarriorApiError& InError, int32 InStatus);

	//로그인 응답 처리. Status 0은 연결 실패(응답 없음)
	void HandleLoginResponse(const FString& InLoginId, int32 Status, const FString& Body);

	//메인화면 값을 계정 데이터로 옮겨 적용한다. 스냅샷에 없는 칸(보상 기록·재화·누적 경험치)은 지금 값을 유지한다
	void ApplyAccountSnapshot(const FWarriorAccountSnapshotDto& InSnapshot);

	//오류 코드(없으면 HTTP 상태)로 화면에 보여 줄 문장을 고른다. 서버 message는 개발 확인용이라 쓰지 않는다
	//InRetryAfterSeconds는 429 잠김의 남은 초(없으면 0)
	static FText LoginErrorToText(const FString& InCode, int32 InStatus, int32 InRetryAfterSeconds = 0);

	//인증이 필요한 요청(접속 점검·로그아웃·/accounts/**)의 401을 처리한다. 401이면 코드로 이유를 골라 EndSession하고 true.
	//401이 아니면 아무것도 하지 않고 false(호출한 쪽이 이어서 처리한다)
	bool HandleAuthFailure(int32 InStatus, const FWarriorApiError& InError);

	//로그아웃 응답 처리. 로그인 상태는 이미 끝났으므로 결과를 로그로만 남긴다
	void HandleLogoutResponse(int32 Status, const FString& Body);

	//로그인 상태를 끝낸다. 토큰·계정·스냅샷 번호를 비우고, 이유를 보관한 뒤 OnSessionEnded로 알린다.
	//이미 로그인하지 않은 상태면 아무것도 하지 않는다(401이 겹쳐 와도 한 번만 처리)
	void EndSession(EWarriorSessionEndReason InReason);

	//종료 이유로 안내 팝업 문구를 고른다. LoggedOut이면 빈 FText
	static FText SessionEndToText(EWarriorSessionEndReason InReason);

	//~ Begin 접속 점검
	//로그인에 성공하면 시작한다. 실패 수를 0으로 두고 HeartbeatIntervalSeconds마다 SendHeartbeat를 부른다.
	//레벨 이동·일시 정지와 상관없이 돌도록 코어 티커(FTSTicker)를 쓴다
	void StartHeartbeat();

	//EndSession·Deinitialize에서 멈춘다
	void StopHeartbeat();

	//티커 콜백. 계속 돌도록 true를 돌려준다
	bool HandleHeartbeatTick(float InDeltaTime);

	//POST {BaseUrl}/auth/heartbeat. 이전 점검의 응답을 아직 기다리는 중이면 이번 차례는 건너뛴다
	void SendHeartbeat();

	//InSessionSerial은 보낼 때의 세션 번호. 그사이 로그아웃·재로그인했다면 지난 세션의 응답이므로 무시한다
	void HandleHeartbeatResponse(int32 InSessionSerial, int32 Status, const FString& Body);
	//~ End 접속 점검

	FWarriorAuthAccount Account;

	//마지막으로 적용한 계정 스냅샷 번호. 이보다 작은 번호의 스냅샷은 오래된 값이므로 무시한다
	int64 LastAppliedAccountVersion = 0;

	//인증 헤더에 붙일 토큰. 로그에 남기지 않는다
	FString AccessToken;

	//로그인 때 받은 세션 만료 시각(참고용). 세션은 인증 요청마다 늘어나므로 판단은 서버의 401로 한다
	FDateTime SessionExpiresAt;

	TMap<FString, FText> LastSignupFieldErrors;

	int32 LastLoginRetryAfterSeconds = 0;

	//아직 화면이 꺼내 가지 않은 종료 이유와 문구(ConsumePendingSessionEnd). None이면 없음
	EWarriorSessionEndReason PendingSessionEndReason = EWarriorSessionEndReason::None;
	FText PendingSessionEndMessage;

	bool bLoggedIn = false;

	bool bRequestInFlight = false;

	//회원가입에 성공하고 로그인 화면이 아직 안내하지 않았다(ConsumeRecentSignup)
	bool bRecentSignup = false;

	//로그인에 성공할 때마다 1씩 오른다. 늦게 온 지난 세션의 응답이 새 세션을 끝내지 않게 비교한다
	int32 SessionSerial = 0;

	FTSTicker::FDelegateHandle HeartbeatTickerHandle;

	//연속으로 실패한 접속 점검 수(로그용). 성공하면 0. 이 수로 로그인을 끝내지 않는다
	int32 HeartbeatFailureCount = 0;

	//실시간 연결이 붙어 있는가(SetRealtimeConnected)
	bool bRealtimeConnected = false;

	//접속 점검 응답을 기다리는 중. 화면 버튼을 막는 bRequestInFlight와 따로 둔다
	bool bHeartbeatInFlight = false;
};
