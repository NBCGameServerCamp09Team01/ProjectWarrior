#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "ProjectWarrior/Chat/WarriorChatRules.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WarriorChatTests
{
	TSharedPtr<FJsonObject> Json(const FString& InText)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InText);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	FWarriorChatLine Sent(const FString& InId, int32 InSecond)
	{
		FWarriorChatLine Line;
		Line.Message.MessageId = InId;
		Line.Message.Scope = EWarriorChatScope::Guild;
		Line.Message.GuildId = TEXT("7");
		Line.Message.SentAt = FDateTime(2026, 10, 16, 12, 0, InSecond);
		return Line;
	}

	FWarriorChatMessage Message(const FString& InId, int32 InSecond)
	{
		return Sent(InId, InSecond).Message;
	}

	TArray<FString> Ids(const TArray<FWarriorChatLine>& InLines)
	{
		TArray<FString> Result;
		for (const FWarriorChatLine& Line : InLines)
		{
			Result.Add(Line.State == EWarriorChatLineState::Sent ? Line.Message.MessageId : TEXT("pending"));
		}
		return Result;
	}
}

// 글 규칙(chat-api.md "규칙"): 코드 포인트 세기·자르기·빈 글·제어 문자·100자.
// 실패하면 WarriorChatRules::CountCodePoints의 서로게이트 쌍 처리와 CheckText 순서를 의심한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorChatTextTest, "ProjectWarrior.Chat.M23.Text",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorChatTextTest::RunTest(const FString& Parameters)
{
	using namespace WarriorChatRules;

	// 이모지(👋 U+1F44B)는 UTF-16 두 칸이지만 한 글자
	const FString Wave = TEXT("\U0001F44B");
	TestEqual(TEXT("Emoji is 2 UTF-16 units"), Wave.Len(), 2);
	TestEqual(TEXT("Emoji is one code point"), CountCodePoints(Wave), 1);
	TestEqual(TEXT("Korean"), CountCodePoints(TEXT("안녕하세요")), 5);
	TestEqual(TEXT("Lone high surrogate counts once"), CountCodePoints(FString(1, TEXT("\xD83D"))), 1);

	FString Trimmed;
	TestEqual(TEXT("Ok"), CheckText(TEXT("  안녕 \U0001F44B  "), 100, Trimmed), EWarriorChatTextCheck::Ok);
	TestEqual(TEXT("Trimmed"), Trimmed, FString(TEXT("안녕 \U0001F44B")));
	TestEqual(TEXT("Empty"), CheckText(TEXT("   "), 100, Trimmed), EWarriorChatTextCheck::Empty);
	TestEqual(TEXT("Newline inside"), CheckText(TEXT("줄\n바꿈"), 100, Trimmed), EWarriorChatTextCheck::ControlChar);
	TestEqual(TEXT("Tab inside"), CheckText(TEXT("a\tb"), 100, Trimmed), EWarriorChatTextCheck::ControlChar);
	TestEqual(TEXT("100 is ok"), CheckText(FString::ChrN(100, TEXT('가')), 100, Trimmed), EWarriorChatTextCheck::Ok);
	TestEqual(TEXT("101 is too long"), CheckText(FString::ChrN(101, TEXT('가')), 100, Trimmed), EWarriorChatTextCheck::TooLong);

	// 100 이모지 = 200 UTF-16 칸이어도 100자라 통과
	FString Emojis;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		Emojis += Wave;
	}
	TestEqual(TEXT("100 emojis are ok"), CheckText(Emojis, 100, Trimmed), EWarriorChatTextCheck::Ok);
	return true;
}

// CHAT_MESSAGE data 읽기(realtime-api.md, chat-api.md "채팅 한 줄"): 전체는 guildId null, 길드는 guildId 필수, 모르는 scope 거절.
// 실패하면 WarriorChatRules::ParseMessage의 칸 이름과 FDateTime::ParseIso8601을 의심한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorChatParseTest, "ProjectWarrior.Chat.M23.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorChatParseTest::RunTest(const FString& Parameters)
{
	using namespace WarriorChatRules;
	using WarriorChatTests::Json;

	FWarriorChatMessage Message;
	TestTrue(TEXT("Guild message"), ParseMessage(Json(TEXT(R"({"messageId":"1042","scope":"GUILD","guildId":"7","senderAccountId":"12","senderNickname":"용사","text":"다들 깼어요?","sentAt":"2026-10-17T05:00:00.120Z"})")), Message));
	TestEqual(TEXT("Id"), Message.MessageId, FString(TEXT("1042")));
	TestEqual(TEXT("Scope"), Message.Scope, EWarriorChatScope::Guild);
	TestEqual(TEXT("Guild"), Message.GuildId, FString(TEXT("7")));
	TestEqual(TEXT("Nickname"), Message.SenderNickname, FString(TEXT("용사")));
	TestEqual(TEXT("SentAt UTC"), Message.SentAt, FDateTime(2026, 10, 17, 5, 0, 0, 120));

	TestTrue(TEXT("Global with null guild"), ParseMessage(Json(TEXT(R"({"messageId":"a1b2","scope":"GLOBAL","guildId":null,"senderAccountId":"3","senderNickname":"n","text":"hi","sentAt":"2026-10-17T05:00:00.000Z"})")), Message));
	TestTrue(TEXT("Global guild empty"), Message.GuildId.IsEmpty());

	TestFalse(TEXT("Guild without guildId"), ParseMessage(Json(TEXT(R"({"messageId":"1","scope":"GUILD","guildId":null,"text":"x","sentAt":"2026-10-17T05:00:00.000Z"})")), Message));
	TestFalse(TEXT("Unknown scope"), ParseMessage(Json(TEXT(R"({"messageId":"1","scope":"WHISPER","text":"x","sentAt":"2026-10-17T05:00:00.000Z"})")), Message));
	TestFalse(TEXT("No messageId"), ParseMessage(Json(TEXT(R"({"scope":"GLOBAL","text":"x","sentAt":"2026-10-17T05:00:00.000Z"})")), Message));
	TestFalse(TEXT("Bad time"), ParseMessage(Json(TEXT(R"({"messageId":"1","scope":"GLOBAL","text":"x","sentAt":"yesterday"})")), Message));
	TestFalse(TEXT("Null object"), ParseMessage(nullptr, Message));
	return true;
}

// 받기 규칙(chat-api.md "게임 처리 규칙"): 다른 길드 버림, 같은 번호 한 번, 50줄 넘으면 옛것부터 버림.
// 실패하면 IsForMyGuild·AppendLine을 의심한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorChatReceiveTest, "ProjectWarrior.Chat.M23.Receive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorChatReceiveTest::RunTest(const FString& Parameters)
{
	using namespace WarriorChatRules;
	using namespace WarriorChatTests;

	const FWarriorChatMessage Mine = Message(TEXT("1"), 0);
	TestTrue(TEXT("My guild"), IsForMyGuild(Mine, TEXT("7")));
	TestFalse(TEXT("Other guild dropped"), IsForMyGuild(Mine, TEXT("8")));
	TestTrue(TEXT("Unknown guild accepted (server sends to members only)"), IsForMyGuild(Mine, FString()));

	TArray<FWarriorChatLine> Lines;
	TestTrue(TEXT("First"), AppendLine(Lines, Sent(TEXT("1"), 0), 50));
	TestFalse(TEXT("Same id once"), AppendLine(Lines, Sent(TEXT("1"), 0), 50));
	TestEqual(TEXT("One line"), Lines.Num(), 1);

	// 내 "보내는 중" 줄은 번호가 없어 겹침 판단에 안 걸린다
	FWarriorChatLine Pending;
	Pending.State = EWarriorChatLineState::Sending;
	Pending.RequestId = TEXT("req");
	TestTrue(TEXT("Pending appended"), AppendLine(Lines, Pending, 50));
	TestTrue(TEXT("Another pending appended"), AppendLine(Lines, Pending, 50));

	Lines.Reset();
	for (int32 Index = 0; Index < 53; ++Index)
	{
		AppendLine(Lines, Sent(FString::FromInt(Index), Index % 60), 50);
	}
	TestEqual(TEXT("Capped at 50"), Lines.Num(), 50);
	TestEqual(TEXT("Oldest dropped"), Lines[0].Message.MessageId, FString(TEXT("3")));
	TestEqual(TEXT("Newest kept"), Lines.Last().Message.MessageId, FString(TEXT("52")));
	return true;
}

// 내역 합치기(CH3): 새것 → 옛것 묶음을 시각 순으로 끼우고 겹침 제거, 보내는 중 줄은 끝, 위로 더 받기는 남은 자리만.
// 실패하면 MergeHistory의 정렬·Room 계산을 의심한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorChatHistoryTest, "ProjectWarrior.Chat.M23.History",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorChatHistoryTest::RunTest(const FString& Parameters)
{
	using namespace WarriorChatRules;
	using namespace WarriorChatTests;

	// 첫 묶음: 비어 있는 탭에 새것 → 옛것으로 와도 옛것 → 새것으로 놓인다
	TArray<FWarriorChatLine> Lines;
	TestEqual(TEXT("Added 3"), MergeHistory(Lines, { Message(TEXT("30"), 30), Message(TEXT("20"), 20), Message(TEXT("10"), 10) }, 50, false), 3);
	TestEqual(TEXT("Ordered"), Ids(Lines), TArray<FString>({ TEXT("10"), TEXT("20"), TEXT("30") }));

	// 다시 붙어 채우기: 겹친 줄(30)은 빼고 새 줄(40)과 그사이 실시간으로 받은 줄은 시각 순, 보내는 중 줄은 끝
	FWarriorChatLine Pending;
	Pending.State = EWarriorChatLineState::Sending;
	Pending.RequestId = TEXT("req");
	Pending.Message.SentAt = FDateTime(2026, 10, 16, 12, 0, 1);
	Lines.Add(Pending);
	AppendLine(Lines, Sent(TEXT("50"), 50), 50);
	TestEqual(TEXT("Added 1"), MergeHistory(Lines, { Message(TEXT("40"), 40), Message(TEXT("30"), 30) }, 50, false), 1);
	TestEqual(TEXT("Merged in time order, pending last"), Ids(Lines),
		TArray<FString>({ TEXT("10"), TEXT("20"), TEXT("30"), TEXT("40"), TEXT("50"), TEXT("pending") }));

	// 위로 더 받기: 남은 자리(상한 8 − 6 = 2)만큼 새것부터
	TestEqual(TEXT("Older fills room only"), MergeHistory(Lines, { Message(TEXT("9"), 9), Message(TEXT("8"), 8), Message(TEXT("7"), 7) }, 8, true), 2);
	TestEqual(TEXT("Older on top"), Ids(Lines),
		TArray<FString>({ TEXT("8"), TEXT("9"), TEXT("10"), TEXT("20"), TEXT("30"), TEXT("40"), TEXT("50"), TEXT("pending") }));
	TestEqual(TEXT("No room"), MergeHistory(Lines, { Message(TEXT("6"), 6) }, 8, true), 0);

	// 첫 묶음이 상한을 넘기면 옛것부터 버린다
	TArray<FWarriorChatLine> Small;
	MergeHistory(Small, { Message(TEXT("3"), 3), Message(TEXT("2"), 2), Message(TEXT("1"), 1) }, 2, false);
	TestEqual(TEXT("Latest capped"), Ids(Small), TArray<FString>({ TEXT("2"), TEXT("3") }));
	return true;
}

// 채팅 문구(chat-api.md "오류"): 429 두 코드는 {Seconds}를 담고, 채팅 맥락의 GUILD_NOT_MEMBER는 채팅 창 문구.
// 실패하면 WarriorErrorText.cpp 채팅 줄·ContextRules를 의심한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorChatErrorTextTest, "ProjectWarrior.Chat.M23.ErrorText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorChatErrorTextTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Code : { TEXT("CHAT_RATE_LIMITED"), TEXT("CHAT_MUTED") })
	{
		const FWarriorErrorRule* Rule = FWarriorErrorText::Find(Code);
		if (TestNotNull(FString::Printf(TEXT("%s row"), Code), Rule))
		{
			TestTrue(FString::Printf(TEXT("%s has {Seconds}"), Code), Rule->Text.ToString().Contains(TEXT("{Seconds}")));
			TestEqual(FString::Printf(TEXT("%s inline"), Code), Rule->Presentation, EWarriorErrorPresentation::Inline);
			const FText Formatted = FText::FormatNamed(Rule->Text, TEXT("Seconds"), FText::AsNumber(42));
			TestTrue(FString::Printf(TEXT("%s formatted"), Code), Formatted.ToString().Contains(TEXT("42")));
		}
	}
	TestNotNull(TEXT("Text invalid row"), FWarriorErrorText::Find(TEXT("CHAT_TEXT_INVALID")));

	FWarriorApiError NotMember;
	NotMember.Code = TEXT("GUILD_NOT_MEMBER");
	NotMember.HttpStatus = 403;
	const FWarriorErrorRule InChat = FWarriorErrorText::Resolve(NotMember, WarriorErrorContext::Chat);
	TestEqual(TEXT("Guild not member in chat is inline"), InChat.Presentation, EWarriorErrorPresentation::Inline);
	TestTrue(TEXT("Guild not member chat text"), InChat.Text.ToString().Contains(TEXT("길드 채팅")));

	FWarriorApiError UnknownChat;
	UnknownChat.Code = TEXT("CHAT_SOMETHING_NEW");
	UnknownChat.HttpStatus = 400;
	TestEqual(TEXT("CHAT_ prefix inline"), FWarriorErrorText::Resolve(UnknownChat).Presentation, EWarriorErrorPresentation::Inline);
	return true;
}

// 채팅 창 계산(U-C2): 날짜선은 PC 날짜가 바뀌는 줄 위(첫 줄은 오늘이 아니면), 시간대 반영, 요일, 입력 글자 자르기.
// 실패하면 WarriorChatRules::BuildRows의 날짜 비교와 TruncateToCodePoints의 서로게이트 처리를 의심한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarriorChatPanelRulesTest, "ProjectWarrior.Chat.M23.PanelRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarriorChatPanelRulesTest::RunTest(const FString& Parameters)
{
	using namespace WarriorChatRules;

	auto LineAt = [](const FDateTime& InUtc)
	{
		FWarriorChatLine Line;
		Line.Message.SentAt = InUtc;
		return Line;
	};
	// UTC 10/15 14:59 = 한국(+9) 10/15 23:59, UTC 10/15 15:01 = 한국 10/16 00:01
	const FTimespan Korea = FTimespan::FromHours(9);
	const TArray<FWarriorChatLine> Lines = {
		LineAt(FDateTime(2026, 10, 15, 14, 0, 0)),
		LineAt(FDateTime(2026, 10, 15, 14, 59, 0)),
		LineAt(FDateTime(2026, 10, 15, 15, 1, 0)),
		LineAt(FDateTime(2026, 10, 15, 15, 2, 0)) };

	// 오늘이 10/16이면: 첫 줄(10/15) 위 날짜선, 자정을 넘긴 줄(10/16) 위 날짜선
	const TArray<FWarriorChatRow> Rows = BuildRows(Lines, Korea, FDateTime(2026, 10, 16, 9, 0, 0));
	TestEqual(TEXT("Rows"), Rows.Num(), 6);
	TestTrue(TEXT("Date line first"), Rows[0].bDateLine);
	TestEqual(TEXT("First date"), Rows[0].LocalDate, FDateTime(2026, 10, 15));
	TestEqual(TEXT("Line 0"), Rows[1].LineIndex, 0);
	TestEqual(TEXT("Line 1 same day"), Rows[2].LineIndex, 1);
	TestTrue(TEXT("Date line at midnight"), Rows[3].bDateLine);
	TestEqual(TEXT("Second date"), Rows[3].LocalDate, FDateTime(2026, 10, 16));
	TestEqual(TEXT("Line 3"), Rows[5].LineIndex, 3);

	// 모두 오늘이면 날짜선 없음
	const TArray<FWarriorChatLine> Today = { LineAt(FDateTime(2026, 10, 16, 1, 0, 0)), LineAt(FDateTime(2026, 10, 16, 2, 0, 0)) };
	TestEqual(TEXT("No date line today"), BuildRows(Today, Korea, FDateTime(2026, 10, 16, 12, 0, 0)).Num(), 2);
	TestEqual(TEXT("Empty"), BuildRows({}, Korea, FDateTime(2026, 10, 16)).Num(), 0);

	// 2026-10-16은 금요일
	TestEqual(TEXT("Friday"), WeekdayKorean(FDateTime(2026, 10, 16)), FString(TEXT("금")));
	TestEqual(TEXT("Sunday"), WeekdayKorean(FDateTime(2026, 10, 18)), FString(TEXT("일")));

	// 입력 자르기: 이모지를 반으로 가르지 않는다
	const FString Wave = TEXT("\U0001F44B");
	TestEqual(TEXT("Under limit kept"), TruncateToCodePoints(TEXT("abc"), 5), FString(TEXT("abc")));
	TestEqual(TEXT("Cut to 2"), TruncateToCodePoints(TEXT("abc"), 2), FString(TEXT("ab")));
	TestEqual(TEXT("Emoji kept whole"), TruncateToCodePoints(TEXT("a") + Wave + TEXT("b"), 2), FString(TEXT("a")) + Wave);
	TestEqual(TEXT("Emoji count after cut"), CountCodePoints(TruncateToCodePoints(Wave + Wave + Wave, 2)), 2);
	return true;
}

#endif
