#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "ProjectWarrior/Network/WarriorGuildName.h"
#include "ProjectWarrior/Network/WarriorGuildSubsystem.h"

// 응답 본문은 루트 docs/contracts/examples/guild-*.json과 같은 모양이다(서버 없이 해석만 본다).
// 실패하면 UWarriorGuildSubsystem::Parse*의 칸 이름(명세 guild-api.md v1)과 null 처리를 먼저 의심한다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorGuildParseMyGuildTest, "ProjectWarrior.Network.M23.GuildParseMyGuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorGuildParseMyGuildTest::RunTest(const FString& Parameters)
{
	// 길드장(examples/guild-me-response.json): 길드·역할·길드원 순서·접속·받은 신청 수·만들기 불가
	const FString LeaderBody = TEXT(R"({"data":{
		"guild":{"guildId":"7","name":"새벽기사단","leaderAccountId":"12","leaderNickname":"용사","memberCount":3,"maxMembers":30,"createdAt":"2026-10-16T06:00:00.123Z","myRole":"LEADER"},
		"members":[
			{"accountId":"12","nickname":"용사","role":"LEADER","joinedAt":"2026-10-16T06:00:00.123Z","online":true},
			{"accountId":"57","nickname":"궁수","role":"MEMBER","joinedAt":"2026-10-16T06:20:41.008Z","online":false}],
		"pendingJoinRequestCount":2,"myJoinRequests":[],
		"create":{"allowed":false,"requirements":[{"type":"STAGE_CLEARED","stageId":"stage.01.01","met":true}]},
		"rejoinAvailableAt":null},
		"meta":{"requestId":"0b6f3c1e-8a2d-4c6e-9f10-2a7b5d9e1c34"}})");

	FWarriorMyGuild Leader;
	if (TestTrue(TEXT("Leader body parsed"), UWarriorGuildSubsystem::ParseMyGuild(LeaderBody, Leader)))
	{
		TestTrue(TEXT("Has guild"), Leader.bHasGuild);
		TestEqual(TEXT("Guild id"), Leader.Guild.GuildId, FString(TEXT("7")));
		TestEqual(TEXT("Guild name"), Leader.Guild.Name, FString(TEXT("새벽기사단")));
		TestEqual(TEXT("Max members"), Leader.Guild.MaxMembers, 30);
		TestEqual(TEXT("My role"), Leader.MyRole, EWarriorGuildRole::Leader);
		TestEqual(TEXT("Members"), Leader.Members.Num(), 2);
		if (Leader.Members.Num() == 2)
		{
			TestEqual(TEXT("Leader first"), Leader.Members[0].Role, EWarriorGuildRole::Leader);
			TestTrue(TEXT("Leader online"), Leader.Members[0].bOnline);
			TestFalse(TEXT("Member offline"), Leader.Members[1].bOnline);
			TestTrue(TEXT("Joined at"), Leader.Members[1].JoinedAt == FDateTime(2026, 10, 16, 6, 20, 41, 8));
		}
		TestEqual(TEXT("Pending count"), Leader.PendingJoinRequestCount, 2);
		TestFalse(TEXT("Cannot create"), Leader.bCanCreate);
		TestEqual(TEXT("Requirements"), Leader.CreateRequirements.Num(), 1);
	}

	// 미소속(examples/guild-me-response-none.json): guild가 null이어도 읽히고, 내 신청이 남는다
	const FString NoneBody = TEXT(R"({"data":{
		"guild":null,"members":[],"pendingJoinRequestCount":0,
		"myJoinRequests":[{"joinRequestId":"105","guildId":"7","guildName":"새벽기사단","accountId":"88","nickname":"초보","requestedAt":"2026-10-16T09:00:00.000Z","expiresAt":"2026-10-19T09:00:00.000Z"}],
		"create":{"allowed":false,"requirements":[{"type":"STAGE_CLEARED","stageId":"stage.01.01","met":false}]},
		"rejoinAvailableAt":null},
		"meta":{"requestId":"0b6f3c1e-8a2d-4c6e-9f10-2a7b5d9e1c34"}})");

	FWarriorMyGuild None;
	if (TestTrue(TEXT("None body parsed"), UWarriorGuildSubsystem::ParseMyGuild(NoneBody, None)))
	{
		TestFalse(TEXT("No guild"), None.bHasGuild);
		TestEqual(TEXT("No role"), None.MyRole, EWarriorGuildRole::None);
		TestEqual(TEXT("No members"), None.Members.Num(), 0);
		TestEqual(TEXT("My requests"), None.MyJoinRequests.Num(), 1);
		if (None.MyJoinRequests.Num() == 1)
		{
			TestEqual(TEXT("Request id"), None.MyJoinRequests[0].JoinRequestId, FString(TEXT("105")));
			TestTrue(TEXT("Expires at"), None.MyJoinRequests[0].ExpiresAt == FDateTime(2026, 10, 19, 9, 0, 0));
		}
		if (None.CreateRequirements.Num() == 1)
		{
			TestTrue(TEXT("Requirement stage"), None.CreateRequirements[0].StageId == FName(TEXT("stage.01.01")));
			TestFalse(TEXT("Requirement not met"), None.CreateRequirements[0].bMet);
		}
	}

	// 못 읽는 본문은 false(화면 값을 바꾸지 않는다)
	FWarriorMyGuild Broken;
	TestFalse(TEXT("Not JSON"), UWarriorGuildSubsystem::ParseMyGuild(TEXT("<html>"), Broken));
	TestFalse(TEXT("No data"), UWarriorGuildSubsystem::ParseMyGuild(TEXT(R"({"meta":{}})"), Broken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorGuildParseListTest, "ProjectWarrior.Network.M23.GuildParseList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorGuildParseListTest::RunTest(const FString& Parameters)
{
	// 마지막 묶음(examples/guild-list-response.json): nextCursor null → 빈 값
	const FString LastPage = TEXT(R"({"data":{"guilds":[
		{"guildId":"9","name":"Knight Order","leaderAccountId":"31","leaderNickname":"창기사","memberCount":30,"maxMembers":30,"createdAt":"2026-10-16T08:12:00.000Z"},
		{"guildId":"7","name":"새벽기사단","leaderAccountId":"12","leaderNickname":"용사","memberCount":3,"maxMembers":30,"createdAt":"2026-10-16T06:00:00.123Z"}],
		"hasNext":false,"nextCursor":null},"meta":{"requestId":"r"}})");

	TArray<FWarriorGuildSummary> Guilds;
	FString Cursor = TEXT("old");
	if (TestTrue(TEXT("Last page parsed"), UWarriorGuildSubsystem::ParseGuildList(LastPage, Guilds, Cursor)))
	{
		TestEqual(TEXT("Guilds"), Guilds.Num(), 2);
		TestTrue(TEXT("No more"), Cursor.IsEmpty());
		if (Guilds.Num() == 2)
		{
			TestEqual(TEXT("Full guild"), Guilds[0].MemberCount, Guilds[0].MaxMembers);
			TestEqual(TEXT("Recent first"), Guilds[0].GuildId, FString(TEXT("9")));
		}
	}

	// 다음 묶음이 있으면 커서를 그대로 들고 있는다
	const FString MidPage = TEXT(R"({"data":{"guilds":[],"hasNext":true,"nextCursor":"opaque-123"},"meta":{"requestId":"r"}})");
	if (TestTrue(TEXT("Mid page parsed"), UWarriorGuildSubsystem::ParseGuildList(MidPage, Guilds, Cursor)))
	{
		TestEqual(TEXT("Cursor kept as is"), Cursor, FString(TEXT("opaque-123")));
	}

	// 받은 신청(examples/guild-join-requests-response.json)
	const FString JoinRequestsBody = TEXT(R"({"data":{"joinRequests":[
		{"joinRequestId":"101","guildId":"7","guildName":"새벽기사단","accountId":"71","nickname":"도적","requestedAt":"2026-10-16T08:40:00.000Z","expiresAt":"2026-10-19T08:40:00.000Z"}]},
		"meta":{"requestId":"r"}})");
	TArray<FWarriorGuildJoinRequest> JoinRequests;
	if (TestTrue(TEXT("Join requests parsed"), UWarriorGuildSubsystem::ParseJoinRequests(JoinRequestsBody, JoinRequests)))
	{
		TestEqual(TEXT("Join requests"), JoinRequests.Num(), 1);
	}
	TestFalse(TEXT("Wrong shape"), UWarriorGuildSubsystem::ParseJoinRequests(LastPage, JoinRequests));

	// 길드 오류 코드: 표에 있고, 모르는 GUILD_ 코드는 길드 기본값(팝업 안 문구, 화면 이동 없음)
	for (const TCHAR* Code : { TEXT("GUILD_NOT_FOUND"), TEXT("GUILD_FULL"), TEXT("GUILD_NAME_DUPLICATED"), TEXT("GUILD_LEADER_CANNOT_LEAVE") })
	{
		TestNotNull(FString::Printf(TEXT("Rule %s"), Code), FWarriorErrorText::Find(Code));
	}
	FWarriorApiError Unknown;
	Unknown.Code = TEXT("GUILD_SOMETHING_NEW");
	Unknown.HttpStatus = 409;
	const FWarriorErrorRule Rule = FWarriorErrorText::Resolve(Unknown, WarriorErrorContext::Guild);
	TestEqual(TEXT("Prefix GUILD_ presentation"), Rule.Presentation, EWarriorErrorPresentation::Inline);
	TestEqual(TEXT("Prefix GUILD_ action"), Rule.Action, EWarriorErrorAction::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorGuildNameTest, "ProjectWarrior.Network.M23.GuildName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 서버 GuildCreateRequest.NAME_PATTERN·GuildQueryService.NAME_PREFIX와 같은 판정인지 본다(서버 GuildSchemaTest·GuildQueryApiTest와 같은 예).
// 실패하면 FWarriorGuildName의 글자 범위(한글 완성형 U+AC00~U+D7A3)·공백 규칙을 먼저 의심한다.
bool FWarriorGuildNameTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Korean"), FWarriorGuildName::Check(TEXT("새벽기사단")).IsValid());
	TestTrue(TEXT("English with one space"), FWarriorGuildName::Check(TEXT("Knight Order")).IsValid());
	TestTrue(TEXT("Two letters"), FWarriorGuildName::Check(TEXT("ab")).IsValid());
	TestTrue(TEXT("Twelve letters"), FWarriorGuildName::Check(TEXT("abcdefghijkl")).IsValid());

	const FWarriorGuildNameCheck One = FWarriorGuildName::Check(TEXT("a"));
	TestFalse(TEXT("One letter is too short"), One.bLengthOk);
	TestFalse(TEXT("Thirteen letters"), FWarriorGuildName::Check(TEXT("abcdefghijklm")).bLengthOk);
	TestFalse(TEXT("Empty"), FWarriorGuildName::Check(FString()).IsValid());

	TestFalse(TEXT("Leading space"), FWarriorGuildName::Check(TEXT(" ab")).bSpacesOk);
	TestFalse(TEXT("Trailing space"), FWarriorGuildName::Check(TEXT("ab ")).bSpacesOk);
	TestFalse(TEXT("Double space"), FWarriorGuildName::Check(TEXT("a  b")).bSpacesOk);
	TestTrue(TEXT("Double space keeps chars ok"), FWarriorGuildName::Check(TEXT("a  b")).bCharsOk);

	TestFalse(TEXT("Jamo"), FWarriorGuildName::Check(TEXT("ㄱㄴ")).bCharsOk);
	TestFalse(TEXT("Symbol"), FWarriorGuildName::Check(TEXT("ab!")).bCharsOk);
	TestFalse(TEXT("Underscore"), FWarriorGuildName::Check(TEXT("a_b")).bCharsOk);

	// 검색어: 1자부터, 첫 글자 공백 금지, 뒤 공백은 서버 정규식이 허용
	TestTrue(TEXT("Prefix one letter"), FWarriorGuildName::IsValidSearchPrefix(TEXT("새")));
	TestTrue(TEXT("Prefix trailing space"), FWarriorGuildName::IsValidSearchPrefix(TEXT("Knight ")));
	TestFalse(TEXT("Prefix leading space"), FWarriorGuildName::IsValidSearchPrefix(TEXT(" a")));
	TestFalse(TEXT("Prefix percent"), FWarriorGuildName::IsValidSearchPrefix(TEXT("a%")));
	TestFalse(TEXT("Prefix too long"), FWarriorGuildName::IsValidSearchPrefix(TEXT("abcdefghijklm")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorGuildParsePresenceTest, "ProjectWarrior.Network.M23.GuildParsePresence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorGuildParsePresenceTest::RunTest(const FString& Parameters)
{
	// 실시간 PRESENCE_CHANGED data(realtime-api.md "메시지 종류"). 실패하면 ParsePresenceChanged의 칸 이름·SetMemberOnline의 찾기를 먼저 의심한다
	auto ReadObject = [](const TCHAR* InJson)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InJson), Object);
		return Object;
	};

	FString GuildId;
	FString AccountId;
	bool bOnline = false;
	if (TestTrue(TEXT("Presence parsed"), UWarriorGuildSubsystem::ParsePresenceChanged(
		ReadObject(TEXT(R"({"guildId":"7","accountId":"57","online":true})")), GuildId, AccountId, bOnline)))
	{
		TestEqual(TEXT("Guild id"), GuildId, FString(TEXT("7")));
		TestEqual(TEXT("Account id"), AccountId, FString(TEXT("57")));
		TestTrue(TEXT("Online"), bOnline);
	}

	// 칸이 빠졌거나 모양이 틀리면 쓰지 않는다(online이 없을 때 끊김으로 보지 않음)
	TestFalse(TEXT("No online"), UWarriorGuildSubsystem::ParsePresenceChanged(
		ReadObject(TEXT(R"({"guildId":"7","accountId":"57"})")), GuildId, AccountId, bOnline));
	TestFalse(TEXT("No account"), UWarriorGuildSubsystem::ParsePresenceChanged(
		ReadObject(TEXT(R"({"guildId":"7","online":false})")), GuildId, AccountId, bOnline));
	TestFalse(TEXT("Online not bool"), UWarriorGuildSubsystem::ParsePresenceChanged(
		ReadObject(TEXT(R"({"guildId":"7","accountId":"57","online":"yes"})")), GuildId, AccountId, bOnline));
	TestFalse(TEXT("No data"), UWarriorGuildSubsystem::ParsePresenceChanged(nullptr, GuildId, AccountId, bOnline));

	// 목록의 그 사람만 바뀐다. 같은 값이면 바뀜 없음, 목록에 없으면 false(G4로 맞춤)
	FWarriorMyGuild MyGuild;
	MyGuild.bHasGuild = true;
	MyGuild.Guild.GuildId = TEXT("7");
	FWarriorGuildMember Leader;
	Leader.AccountId = TEXT("12");
	Leader.bOnline = true;
	FWarriorGuildMember Member;
	Member.AccountId = TEXT("57");
	MyGuild.Members = { Leader, Member };

	bool bChanged = false;
	TestTrue(TEXT("Member found"), UWarriorGuildSubsystem::SetMemberOnline(MyGuild, TEXT("57"), true, bChanged));
	TestTrue(TEXT("Member changed"), bChanged);
	TestTrue(TEXT("Member now online"), MyGuild.Members[1].bOnline);
	TestTrue(TEXT("Leader untouched"), MyGuild.Members[0].bOnline);

	TestTrue(TEXT("Same value found"), UWarriorGuildSubsystem::SetMemberOnline(MyGuild, TEXT("57"), true, bChanged));
	TestFalse(TEXT("Same value not changed"), bChanged);

	TestFalse(TEXT("Unknown member"), UWarriorGuildSubsystem::SetMemberOnline(MyGuild, TEXT("99"), false, bChanged));
	TestFalse(TEXT("Unknown member not changed"), bChanged);
	return true;
}

#endif
