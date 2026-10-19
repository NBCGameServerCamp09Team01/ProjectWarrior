#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorPendingRequestStore.h"
#include "ProjectWarrior/Network/WarriorStagePlaySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorErrorTextTest, "ProjectWarrior.Network.S5.ErrorText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorErrorTextTest::RunTest(const FString& Parameters)
{
	// 표 자체: 코드가 겹치지 않고 문구가 비어 있지 않다.
	TSet<FString> Codes;
	for (const FWarriorErrorRule& Rule : FWarriorErrorText::GetAllRules())
	{
		TestFalse(FString::Printf(TEXT("Duplicated code %s"), *Rule.Code), Codes.Contains(Rule.Code));
		Codes.Add(Rule.Code);
		TestFalse(FString::Printf(TEXT("Empty text for %s"), *Rule.Code), Rule.Text.IsEmpty());
	}

	// 서버가 retryable true로 주는 코드는 다음 행동이 Retry다(README 공통 규칙).
	for (const TCHAR* RetryCode : { TEXT("INTERNAL_ERROR"), TEXT("SERVICE_UNAVAILABLE"), TEXT("VERSION_CONFLICT"), TEXT("REQUEST_IN_PROGRESS"), TEXT("NETWORK_ERROR") })
	{
		const FWarriorErrorRule* Rule = FWarriorErrorText::Find(RetryCode);
		if (TestNotNull(FString::Printf(TEXT("Rule %s"), RetryCode), Rule))
		{
			TestEqual(FString::Printf(TEXT("%s action"), RetryCode), Rule->Action, EWarriorErrorAction::Retry);
		}
	}

	// 찾는 순서: 접두어 → 상태 → 알 수 없는 오류.
	FWarriorApiError Error;
	Error.Code = TEXT("STAGE_PLAY_SOMETHING_NEW");
	Error.HttpStatus = 409;
	TestEqual(TEXT("Prefix STAGE_PLAY_"), FWarriorErrorText::Resolve(Error).Action, EWarriorErrorAction::DiscardStored);

	Error.Code = TEXT("HTTP_502");
	Error.HttpStatus = 502;
	TestEqual(TEXT("Unparsed 5xx uses INTERNAL_ERROR"), FWarriorErrorText::Resolve(Error).Action, EWarriorErrorAction::Retry);
	TestEqual(TEXT("Unparsed 5xx keeps its code"), FWarriorErrorText::Resolve(Error).Code, FString(TEXT("HTTP_502")));

	// 로그인 화면: 본문 없는 5xx는 서버 문구, 모르는 4xx는 로그인 문구.
	TestEqual(TEXT("Login 5xx text"), FWarriorErrorText::Resolve(Error, WarriorErrorContext::Login).Text.ToString(),
		FWarriorErrorText::GetText(TEXT("INTERNAL_ERROR")).ToString());
	Error.Code = TEXT("HTTP_404");
	Error.HttpStatus = 404;
	TestEqual(TEXT("Login unknown 4xx text"), FWarriorErrorText::Resolve(Error, WarriorErrorContext::Login).Text.ToString(),
		FString(TEXT("로그인하지 못했습니다. 잠시 뒤 다시 시도해 주세요.")));

	// {Code}가 코드로 바뀐다.
	Error.Code = TEXT("SOMETHING_ELSE");
	Error.HttpStatus = 400;
	TestTrue(TEXT("Unknown text has code"), FWarriorErrorText::Resolve(Error).Text.ToString().Contains(TEXT("SOMETHING_ELSE")));

	// 재시도 판단.
	FWarriorApiError Network = UWarriorApiSubsystem::ParseError(0, FString());
	TestTrue(TEXT("Network is retryable"), UWarriorApiSubsystem::IsRetryable(Network));
	TestTrue(TEXT("Bodyless 502 is retryable"), UWarriorApiSubsystem::IsRetryable(UWarriorApiSubsystem::ParseError(502, TEXT("<html>"))));
	const FWarriorApiError Expired = UWarriorApiSubsystem::ParseError(409,
		TEXT("{\"code\":\"STAGE_PLAY_EXPIRED\",\"message\":\"m\",\"path\":\"/p\",\"retryable\":false}"));
	TestEqual(TEXT("Parsed code"), Expired.Code, FString(TEXT("STAGE_PLAY_EXPIRED")));
	TestFalse(TEXT("Expired is not retryable"), UWarriorApiSubsystem::IsRetryable(Expired));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorPendingStoreTest, "ProjectWarrior.Network.S5.PendingStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorPendingStoreTest::RunTest(const FString& Parameters)
{
	// 시험 전용 계정 폴더를 쓰고 끝나면 지운다.
	const FString AccountId = FString::Printf(TEXT("automation-%s"), *FGuid::NewGuid().ToString());
	const FString Dir = FWarriorPendingRequestStore::GetAccountDir(AccountId);
	TestFalse(TEXT("Account id is not in the folder name"), Dir.Contains(AccountId));

	FWarriorPendingRequest Entry;
	Entry.Kind = EWarriorPendingKind::StageResult;
	Entry.Verb = TEXT("POST");
	Entry.Path = TEXT("/accounts/me/stage-plays/3f2b8c1e-7a4d-4e2f-9b10-6c5d4e3f2a1b/result");
	Entry.Body = TEXT("{\"requestId\":\"aaaa\",\"cleared\":true}");
	Entry.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Entry.StoreKey = TEXT("3f2b8c1e-7a4d-4e2f-9b10-6c5d4e3f2a1b");
	Entry.CreatedAt = FDateTime::UtcNow();
	Entry.ExpiresAt = Entry.CreatedAt + FTimespan::FromHours(24);

	TestTrue(TEXT("Save"), FWarriorPendingRequestStore::Save(AccountId, Entry));

	int32 Corrupted = 0;
	TArray<FWarriorPendingRequest> Loaded = FWarriorPendingRequestStore::LoadAll(AccountId, Corrupted);
	TestEqual(TEXT("Loaded one"), Loaded.Num(), 1);
	TestEqual(TEXT("No corrupted"), Corrupted, 0);
	if (Loaded.Num() == 1)
	{
		TestEqual(TEXT("Same body"), Loaded[0].Body, Entry.Body);
		TestEqual(TEXT("Same kind"), Loaded[0].Kind, EWarriorPendingKind::StageResult);
	}

	FWarriorPendingRequest Found;
	TestTrue(TEXT("FindByKey"), FWarriorPendingRequestStore::FindByKey(AccountId, EWarriorPendingKind::StageResult, Entry.StoreKey, Found));
	TestFalse(TEXT("Another account cannot see it"), FWarriorPendingRequestStore::FindByKey(AccountId + TEXT("x"), EWarriorPendingKind::StageResult, Entry.StoreKey, Found));

	// 파일 한 글자를 바꾸면 깨진 파일로 보고 보내지 않는다.
	const FString Path = FPaths::Combine(Dir, Entry.RequestId + TEXT(".json"));
	FString Json;
	FFileHelper::LoadFileToString(Json, *Path);
	Json.ReplaceInline(TEXT("\"cleared\\\":true"), TEXT("\"cleared\\\":false"));
	FFileHelper::SaveStringToFile(Json, *Path);
	Loaded = FWarriorPendingRequestStore::LoadAll(AccountId, Corrupted);
	TestEqual(TEXT("Corrupted not loaded"), Loaded.Num(), 0);
	TestEqual(TEXT("Corrupted counted"), Corrupted, 1);
	TestTrue(TEXT("Moved to .bad"), IFileManager::Get().FileExists(*(Path + TEXT(".bad"))));

	// 지우기.
	TestTrue(TEXT("Save again"), FWarriorPendingRequestStore::Save(AccountId, Entry));
	TestTrue(TEXT("Remove"), FWarriorPendingRequestStore::Remove(AccountId, Entry.RequestId));
	Loaded = FWarriorPendingRequestStore::LoadAll(AccountId, Corrupted);
	TestEqual(TEXT("Removed"), Loaded.Num(), 0);

	// 경로를 벗어나는 이름은 저장하지 않는다(Error 로그가 나오는 것이 맞다).
	AddExpectedError(TEXT("bad requestId"), EAutomationExpectedErrorFlags::Contains, 1);
	Entry.RequestId = TEXT("../evil");
	TestFalse(TEXT("Unsafe requestId rejected"), FWarriorPendingRequestStore::Save(AccountId, Entry));

	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorResultSubmitStateTest, "ProjectWarrior.Network.S5.ResultSubmitState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorResultSubmitStateTest::RunTest(const FString& Parameters)
{
	// 결과 화면은 Saved·Rejected면 보이고, Offline·SessionEnded면 보이지 않는다(AWarriorStagePlayerController).
	FWarriorApiResult Saved;
	Saved.bOk = true;
	Saved.HttpStatus = 200;
	TestEqual(TEXT("200 is saved"), UWarriorStagePlaySubsystem::ClassifyResultResponse(Saved), EWarriorResultSubmitState::Saved);

	// 연결 실패로 끝남("보관 후 종료" 또는 다시 시도도 실패): 보관했다.
	FWarriorApiResult Offline;
	Offline.HttpStatus = 0;
	Offline.Error = UWarriorApiSubsystem::ParseError(0, FString());
	Offline.bStored = true;
	TestEqual(TEXT("Network error is offline"), UWarriorStagePlaySubsystem::ClassifyResultResponse(Offline), EWarriorResultSubmitState::Offline);

	// 보관에 실패했어도 연결이 안 된 것은 같다.
	Offline.bStored = false;
	TestEqual(TEXT("Network error without store is offline"), UWarriorStagePlaySubsystem::ClassifyResultResponse(Offline), EWarriorResultSubmitState::Offline);

	// 본문 없는 5xx도 다시 보낼 실패라 연결 안 됨으로 본다.
	FWarriorApiResult Bodyless;
	Bodyless.HttpStatus = 502;
	Bodyless.Error = UWarriorApiSubsystem::ParseError(502, TEXT("<html>"));
	TestEqual(TEXT("Bodyless 502 is offline"), UWarriorStagePlaySubsystem::ClassifyResultResponse(Bodyless), EWarriorResultSubmitState::Offline);

	// 서버가 받지 않음: 연결은 됐다.
	FWarriorApiResult Rejected;
	Rejected.HttpStatus = 409;
	Rejected.Error = UWarriorApiSubsystem::ParseError(409,
		TEXT("{\"code\":\"STAGE_PLAY_EXPIRED\",\"message\":\"m\",\"path\":\"/p\",\"retryable\":false}"));
	TestEqual(TEXT("Expired is rejected"), UWarriorStagePlaySubsystem::ClassifyResultResponse(Rejected), EWarriorResultSubmitState::Rejected);

	// 401: 보관했어도 세션 종료가 먼저다(메인메뉴로 간다).
	FWarriorApiResult Unauthorized;
	Unauthorized.HttpStatus = 401;
	Unauthorized.Error = UWarriorApiSubsystem::ParseError(401,
		TEXT("{\"code\":\"AUTH_SESSION_NOT_FOUND\",\"message\":\"m\",\"path\":\"/p\",\"retryable\":false}"));
	Unauthorized.bStored = true;
	TestEqual(TEXT("401 is session ended"), UWarriorStagePlaySubsystem::ClassifyResultResponse(Unauthorized), EWarriorResultSubmitState::SessionEnded);

	return true;
}

#endif
