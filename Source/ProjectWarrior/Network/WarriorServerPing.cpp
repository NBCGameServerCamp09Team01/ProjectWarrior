#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ProjectWarrior/ProjectWarrior.h"

/**
 * 웹서버 연결 확인용 콘솔 명령 (디버그용).
 *   Server.Ping              http://localhost:8080/actuator/health 호출
 *   Server.Ping <BaseUrl>    다른 주소로 호출 (예: Server.Ping http://127.0.0.1:8080)
 * 200 {"status":"UP"}이면 게임 → 서버 HTTP 연결과 서버의 DB·Redis 연결이 모두 정상이다.
 */
namespace WarriorServerPing
{
	void Ping(const TArray<FString>& Args)
	{
		const FString BaseUrl = Args.Num() > 0 ? Args[0] : TEXT("http://localhost:8080");
		const FString Url = BaseUrl + TEXT("/actuator/health");

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb(TEXT("GET"));
		Request->SetTimeout(5.f);
		Request->OnProcessRequestComplete().BindLambda([Url](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				UE_LOG(LogProjectWarrior, Warning, TEXT("[Server] Ping FAILED. Cannot reach %s (server off? wrong port?)"), *Url);
				return;
			}

			UE_LOG(LogProjectWarrior, Display, TEXT("[Server] Ping %d %s -> %s"),
				Response->GetResponseCode(), *Url, *Response->GetContentAsString());
		});
		Request->ProcessRequest();

		UE_LOG(LogProjectWarrior, Display, TEXT("[Server] Ping sent to %s"), *Url);
	}

	FAutoConsoleCommand PingCommand(
		TEXT("Server.Ping"), TEXT("웹서버 연결 확인 (GET /actuator/health). 예: Server.Ping http://localhost:8080"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Ping));
}

#endif // !UE_BUILD_SHIPPING
