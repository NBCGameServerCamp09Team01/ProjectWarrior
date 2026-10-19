#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "JsonObjectConverter.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "WarriorAccountTypes.h"

/**
 * 계정 데이터 JSON 확인용 콘솔 명령 (디버그용).
 *   Account.DumpJson   기본값 계정과 샘플 값 계정을 FJsonObjectConverter로 바꿔 로그에 출력한다
 * 서버 응답 본문과 FWarriorAccountData의 모양을 비교할 때 쓴다.
 */
namespace WarriorAccountJsonDump
{
	void LogAsJson(const TCHAR* Label, const FWarriorAccountData& Data)
	{
		FString Json;
		if (!FJsonObjectConverter::UStructToJsonObjectString(Data, Json))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Account] DumpJson failed. %s"), Label);
			return;
		}

		UE_LOG(LogProjectWarrior, Display, TEXT("[Account] DumpJson %s:\n%s"), Label, *Json);
	}

	void Dump()
	{
		LogAsJson(TEXT("default"), FWarriorAccountData());

		FWarriorAccountData Sample;
		Sample.AccountLevel = 3;
		Sample.Experience = 120;
		Sample.StatPoints = 2;
		Sample.InvestedStats.Add(FGameplayTag::RequestGameplayTag(TEXT("Account.Stat.AttackPower")), 3);
		Sample.InvestedStats.Add(FGameplayTag::RequestGameplayTag(TEXT("Account.Stat.MaxHealth")), 1);
		Sample.UnlockedSkills.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Account.Skill.DodgeMastery")));
		Sample.UnlockedSkills.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Account.Skill.HeavyAttack")));
		Sample.ExternalCurrencies.Add(TEXT("Gold"), 1500);
		Sample.ExternalCurrencies.Add(TEXT("BigValue"), 9007199254740993LL);	// 2^53 + 1, double 정밀도 확인용
		Sample.RewardedRecordIds.Add(FGuid::NewGuid());
		LogAsJson(TEXT("sample"), Sample);
	}

	FAutoConsoleCommand DumpJsonCommand(
		TEXT("Account.DumpJson"), TEXT("계정 데이터를 JSON으로 바꿔 로그에 출력한다 (디버그용)"),
		FConsoleCommandDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
