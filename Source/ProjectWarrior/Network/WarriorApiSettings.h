// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WarriorApiSettings.generated.h"

class UWarriorNoticePopupWidget;

/**
 * 프로젝트 설정 > Game > Warrior Api. 서버 요청 실패 처리(S5)의 값을 모은다. 값은 Config/DefaultGame.ini에 저장된다.
 * 재시도 횟수·간격을 코드에 숫자로 쓰지 않고 여기서 바꾼다(10/10 결정).
 * 서버 주소·요청 제한 시간은 UWarriorAuthSubsystem(BaseUrl·PackagedBaseUrl → GetEffectiveBaseUrl, RequestTimeoutSeconds)에 있다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Warrior Api"))
class PROJECTWARRIOR_API UWarriorApiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UWarriorApiSettings* Get() { return GetDefault<UWarriorApiSettings>(); }

	//서버 장애(5xx·retryable true) 때 자동 재시도 간격(초). 개수가 자동 재시도 횟수다. 비우면 자동 재시도를 하지 않는다
	UPROPERTY(Config, EditAnywhere, Category = "Retry")
	TArray<float> AutoRetryDelaysSeconds = { 1.f, 3.f };

	//응답이 없을 때(서버가 꺼짐·연결 실패) 자동 재시도 간격(초). 연결 시도 한 번에 몇 초가 걸리므로 기본은 비워 두고 바로 묻는다.
	//(10/12 손 시험: 재시도 2번이면 팝업까지 약 10초, 결과 화면을 떠난 뒤에 뜸)
	UPROPERTY(Config, EditAnywhere, Category = "Retry")
	TArray<float> NetworkErrorRetryDelaysSeconds;

	//자동 재시도가 모두 실패한 뒤 "다시 시도 / 종료"를 한 번 물을지(묻기를 켠 요청만). 끄면 바로 보관 후 종료 안내
	UPROPERTY(Config, EditAnywhere, Category = "Retry")
	bool bAskRetryAfterAutoRetry = true;

	//보관한 요청의 기본 유효 기간(시간). 요청이 따로 만료 시각을 주면(결과 제출: 플레이 마감) 그 값을 쓴다
	UPROPERTY(Config, EditAnywhere, Category = "Pending", meta = (ClampMin = "1.0", Units = "Hours"))
	float PendingDefaultTtlHours = 24.f;

	//타이틀에서 서버 상태를 확인할지(E6). 실패하면 안내 후 종료한다
	UPROPERTY(Config, EditAnywhere, Category = "Server Check")
	bool bCheckServerOnTitle = true;

	//서버 상태 확인 경로(인증 없음). 웹서버는 actuator health만 노출한다
	UPROPERTY(Config, EditAnywhere, Category = "Server Check")
	FString HealthCheckPath = TEXT("/actuator/health");

	//서버 상태 확인 제한 시간(초)
	UPROPERTY(Config, EditAnywhere, Category = "Server Check", meta = (ClampMin = "1.0", Units = "s"))
	float HealthCheckTimeoutSeconds = 5.f;

	//오류 팝업 위젯. 비어 있으면 팝업 없이 로그만 남긴다(예: WBP_NoticePopup)
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UWarriorNoticePopupWidget> ErrorPopupClass;
};
