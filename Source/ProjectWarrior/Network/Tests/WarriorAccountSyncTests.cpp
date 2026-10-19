#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Network/WarriorAccountSync.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorAccountSyncTest, "ProjectWarrior.Network.M25.AccountSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorAccountSyncTest::RunTest(const FString& Parameters)
{
	using namespace WarriorAccountSync;

	// 결과 제출·보관 결과 재전송 응답: 보낼 때와 같은 세션·같은 계정일 때만 적용한다(account-api.md "받는 쪽 규칙").
	TestTrue(TEXT("Same session and account"), IsSameSession(true, 3, 3, TEXT("7"), TEXT("7")));
	TestFalse(TEXT("Logged out"), IsSameSession(false, 3, 3, TEXT("7"), TEXT("7")));
	TestFalse(TEXT("Logged in again"), IsSameSession(true, 4, 3, TEXT("7"), TEXT("7")));
	TestFalse(TEXT("Other account"), IsSameSession(true, 3, 3, TEXT("7"), TEXT("8")));
	TestFalse(TEXT("No account in response"), IsSameSession(true, 3, 3, TEXT("7"), FString()));
	TestFalse(TEXT("Unknown sent account"), IsSameSession(true, 3, 3, FString(), FString()));

	// 모든 응답: 작은 version은 버리고 같거나 크면 적용한다(account-api.md "버전 규칙"). 로그아웃하면 기준은 0.
	TestFalse(TEXT("Older version"), IsNotOlder(5, 6));
	TestTrue(TEXT("Same version"), IsNotOlder(6, 6));
	TestTrue(TEXT("Newer version"), IsNotOlder(7, 6));
	TestTrue(TEXT("First apply after logout"), IsNotOlder(0, 0));
	return true;
}

#endif
