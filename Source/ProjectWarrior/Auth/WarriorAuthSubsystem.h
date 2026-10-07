// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WarriorAuthTypes.h"
#include "WarriorAuthSubsystem.generated.h"

//요청 결과. 실패하면 ErrorCode에 서버 오류 코드(예: AUTH_INVALID_CREDENTIALS), Message에 화면에 보여 줄 문장이 들어온다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWarriorAuthRequestCompleted, bool, bSuccess, const FString&, ErrorCode, const FText&, Message);

/**
 * 인증 서브시스템. 회원가입·로그인을 요청하고 로그인 상태(계정, 액세스 토큰)를 보관한다.
 * 명세: 루트 docs/contracts/auth-api.md
 *
 * - 로그인·회원가입 화면은 Request* 를 호출하고 OnLoginCompleted·OnSignupCompleted로 결과를 받는다.
 * - 프론트 컨트롤러는 IsLoggedIn으로 메인메뉴에 들어갈 수 있는지 판단한다.
 * - 입력 규칙(길이·문자)은 Validate* 에 모아 두고, 화면이 요청 전에 먼저 확인한다(서버도 다시 검증한다).
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

	//POST {BaseUrl}/auth/signup. 결과는 OnSignupCompleted로 알린다.
	//가입만 하고 로그인은 하지 않는다(토큰을 주지 않으므로 이어서 로그인한다). 성공하면 아이디를 기억해 로그인 화면이 채운다
	//InEmail은 선택이다. 비어 있으면 JSON에서 칸을 빼고 보낸다(명세: 비워 두거나 null)
	//비밀번호 확인 칸은 화면에서만 비교하고 여기로 넘기지 않는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void RequestSignup(const FString& InLoginId, const FString& InPassword, const FString& InNickname, const FString& InEmail);

	//POST {BaseUrl}/auth/login. 결과는 OnLoginCompleted로 알린다.
	//성공하면 토큰을 보관하고 메인화면 값을 계정 서브시스템에 적용(ApplyServerSnapshot)한 뒤 성공을 알린다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Auth")
	void RequestLogin(const FString& InLoginId, const FString& InPassword);

	UFUNCTION(BlueprintPure, Category = "Warrior|Auth")
	bool IsLoggedIn() const { return bLoggedIn; }

	//요청을 보내고 결과를 기다리는 중인가. 그동안 화면은 버튼을 막는다
	UFUNCTION(BlueprintPure, Category = "Warrior|Auth")
	bool IsRequestInFlight() const { return bRequestInFlight; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Auth")
	const FWarriorAuthAccount& GetAccount() const { return Account; }

	//방금 가입한 아이디를 한 번 꺼낸다. 로그인 화면이 아이디 칸을 채우고 안내 문구를 띄울 때 쓴다
	bool ConsumeRecentSignupLoginId(FString& OutLoginId);

	//마지막 회원가입 실패의 칸별 오류. 키는 서버 칸 이름(loginId, nickname, email, password), 값은 화면 문구.
	//400 VALIDATION_FAILED의 errors[]와 409 중복(아이디·닉네임)을 담는다. 회원가입 화면이 해당 칸 아래에 표시한다
	const TMap<FString, FText>& GetLastSignupFieldErrors() const { return LastSignupFieldErrors; }

	//~ Begin 입력 규칙. 문제가 없으면 빈 FText, 있으면 화면에 보여 줄 문장을 돌려준다
	//길이·문자는 명세의 제안 값이다. 아이디 대소문자 구분 없음, 아이디·닉네임 중복은 서버가 판정한다(409)
	static FText ValidateLoginId(const FString& InLoginId);
	static FText ValidatePassword(const FString& InPassword);
	static FText ValidateNickname(const FString& InNickname);
	//비어 있으면 통과(선택 입력). 값이 있으면 형식만 간단히 본다
	static FText ValidateEmail(const FString& InEmail);
	//~ End 입력 규칙

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Auth")
	FOnWarriorAuthRequestCompleted OnSignupCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Auth")
	FOnWarriorAuthRequestCompleted OnLoginCompleted;

protected:
	//웹서버 주소 (끝에 / 없이). 경로는 /auth/login처럼 붙인다
	UPROPERTY(Config)
	FString BaseUrl = TEXT("http://localhost:8080");

	//요청 제한 시간(초). 넘으면 연결 실패(NETWORK_ERROR)로 처리한다
	UPROPERTY(Config)
	float RequestTimeoutSeconds = 10.f;

private:
	//JSON 본문으로 POST {BaseUrl}{InPath}를 보낸다. 응답(또는 연결 실패)이 오면 OnDone(Status, Body)를 부른다. Status 0은 연결 실패.
	//요청을 시작하지 못하면 false. 본문에 비밀번호가 있을 수 있으므로 본문은 로그에 남기지 않는다
	bool SendJsonPost(const FString& InPath, const FString& InJsonBody, TFunction<void(int32 /*Status*/, const FString& /*Body*/)>&& OnDone);

	//실패 응답 본문을 읽는다. 연결 실패(Status 0)는 NETWORK_ERROR, 본문을 못 읽으면 HTTP_<상태>로 채운다
	static FWarriorApiError ParseApiError(int32 InStatus, const FString& InBody);

	//회원가입·로그인이 같이 쓰는 문구(서버 장애, 연결 실패). 해당하지 않으면 빈 FText
	static FText CommonErrorToText(const FString& InCode, int32 InStatus);

	//회원가입 응답 처리. 201이면 성공
	void HandleSignupResponse(const FString& InLoginId, int32 Status, const FString& Body);

	//오류 코드로 회원가입 화면 문구를 고른다. VALIDATION_FAILED는 칸별 오류(errors[].message)를 보여 준다
	static FText SignupErrorToText(const FWarriorApiError& InError, int32 InStatus);

	//로그인 응답 처리. Status 0은 연결 실패(응답 없음)
	void HandleLoginResponse(const FString& InLoginId, int32 Status, const FString& Body);

	//메인화면 값을 계정 데이터로 옮겨 적용한다. 스냅샷에 없는 칸(보상 기록·재화·누적 경험치)은 지금 값을 유지한다
	void ApplyAccountSnapshot(const FWarriorAccountSnapshotDto& InSnapshot);

	//오류 코드(없으면 HTTP 상태)로 화면에 보여 줄 문장을 고른다. 서버 message는 개발 확인용이라 쓰지 않는다
	static FText LoginErrorToText(const FString& InCode, int32 InStatus);

	FWarriorAuthAccount Account;

	//마지막으로 적용한 계정 스냅샷 번호. 이보다 작은 번호의 스냅샷은 오래된 값이므로 무시한다
	int64 LastAppliedAccountVersion = 0;

	//인증 헤더에 붙일 토큰. 로그에 남기지 않는다
	FString AccessToken;

	FDateTime AccessTokenExpiresAt;

	FString RecentSignupLoginId;

	TMap<FString, FText> LastSignupFieldErrors;

	bool bLoggedIn = false;

	bool bRequestInFlight = false;
};
