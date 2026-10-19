#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorApiSettings.h"

/**
 * 웹서버 연결 확인용 콘솔 명령 (디버그용). 게임(PIE·Standalone·패키지 Development) 중 ~ 콘솔에서 입력한다.
 *   Server.Ping              지금 쓰는 서버 주소(인증 서브시스템의 GetEffectiveBaseUrl) + 설정의 서버 확인 경로(HealthCheckPath)
 *   Server.Ping <주소>        다른 주소로 호출 (예: Server.Ping http://127.0.0.1:8080). 경로는 같은 설정 값
 * 타이틀의 서버 확인과 같은 주소·경로를 부르므로, 200이면 타이틀 확인도 통과한다.
 */
namespace WarriorServerPing
{
	void Ping(const TArray<FString>& Args, UWorld* World)
	{
		//주소: 인자가 있으면 그 주소, 없으면 게임이 지금 쓰는 서버 주소(실행 인자·패키지·에디터에 따라 다름)
		FString BaseUrl;
		if (Args.Num() > 0)
		{
			BaseUrl = Args[0];
		}
		else if (const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(World))
		{
			BaseUrl = Auth->GetEffectiveBaseUrl();
		}
		else
		{
			//에디터에서 게임을 띄우지 않은 상태(GameInstance 없음)
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Server] Ping ignored. No game running. Start PIE or give an address: Server.Ping http://localhost:8080"));
			return;
		}
		BaseUrl.RemoveFromEnd(TEXT("/"));

		const FString Url = BaseUrl + UWarriorApiSettings::Get()->HealthCheckPath;

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb(TEXT("GET"));
		Request->SetTimeout(5.f);
		Request->OnProcessRequestComplete().BindLambda([Url](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				UE_LOG(LogProjectWarrior, Warning, TEXT("[Server] Ping FAILED. Cannot reach %s (server off? wrong address?)"), *Url);
				return;
			}

			UE_LOG(LogProjectWarrior, Display, TEXT("[Server] Ping %d %s -> %s"),
				Response->GetResponseCode(), *Url, *Response->GetContentAsString());
		});
		Request->ProcessRequest();

		UE_LOG(LogProjectWarrior, Display, TEXT("[Server] Ping sent to %s"), *Url);
	}

	FAutoConsoleCommandWithWorldAndArgs PingCommand(
		TEXT("Server.Ping"), TEXT("웹서버 연결 확인 (지금 서버 주소 + 설정의 서버 확인 경로). 예: Server.Ping / Server.Ping http://localhost:8080"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Ping));
}

#endif // !UE_BUILD_SHIPPING
