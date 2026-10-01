#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "WarriorAccountSubsystem.h"

/**
 * 계정 테스트용 콘솔 명령 (SK 테스트). PIE 중 ~ 콘솔에서 입력한다.
 *   Account.AddStatPoints N   스탯 포인트를 N 더한다 (기본 1)
 * 계정 서브시스템 코드를 고치지 않으려고 ApplyServerSnapshot으로 값을 넣는다.
 * 그래서 그 시점까지의 변경 내역이 서버 확인됨으로 표시된다 (디버그 전용이라 무시).
 */
namespace WarriorAccountConsole
{
	void AddStatPoints(const TArray<FString>& Args, UWorld* World)
	{
		UWarriorAccountSubsystem* Account = UWarriorAccountSubsystem::Get(World);
		if (!Account)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] AddStatPoints ignored. No account subsystem."));
			return;
		}

		const int32 Points = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		if (Points <= 0)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] AddStatPoints ignored. Points must be positive. Usage: Account.AddStatPoints 5"));
			return;
		}

		FWarriorAccountData NewData = Account->GetAccountData();
		NewData.StatPoints += Points;
		Account->ApplyServerSnapshot(NewData);

		UE_LOG(LogProjectWarrior, Display, TEXT("[Account] Debug: added %d stat points. Now %d."), Points, NewData.StatPoints);
	}

	FAutoConsoleCommandWithWorldAndArgs AddStatPointsCommand(
		TEXT("Account.AddStatPoints"), TEXT("스탯 포인트를 N 더한다 (디버그용). 예: Account.AddStatPoints 5"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddStatPoints));
}

#endif // !UE_BUILD_SHIPPING
