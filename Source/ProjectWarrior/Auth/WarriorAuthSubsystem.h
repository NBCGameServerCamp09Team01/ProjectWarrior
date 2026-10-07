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
 * 로그인은 웹서버에 실제로 요청한다(POST {BaseUrl}/auth/login). 회원가입은 아직 임시 구현이다(TODO(server) 표시).
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

	//POST /auth/signup. 가입만 하고 로그인은 하지 않는다(명세대로 이어서 로그인을 부른다)
	//InEmail은 선택이다. 비어 있으면 서버에 보내지 않는다(명세: 비워 두거나 null)
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
	//TODO(server): 회원가입은 아직 임시 응답. 잠깐 기다린 뒤 성공으로 처리한다
	void CompleteSignupPlaceholder(FString InLoginId);

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

	FTimerHandle PlaceholderTimer;

	bool bLoggedIn = false;

	bool bRequestInFlight = false;
};
