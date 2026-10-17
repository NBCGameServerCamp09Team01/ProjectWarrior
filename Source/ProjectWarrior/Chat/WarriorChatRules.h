// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Chat/WarriorChatTypes.h"

class FJsonObject;

//채팅 창 목록의 한 칸: 날짜선 또는 채팅 줄(Lines의 번호)
struct FWarriorChatRow
{
	bool bDateLine = false;
	//날짜선이면 그 날짜(PC 시간대)
	FDateTime LocalDate;
	//채팅 줄이면 Lines 안 번호, 날짜선이면 INDEX_NONE
	int32 LineIndex = INDEX_NONE;
};

/**
 * 채팅 규칙 중 화면·서버와 상관없는 계산(자동 시험 대상). 명세: 루트 docs/contracts/chat-api.md "규칙"·"게임 처리 규칙"
 * UWarriorChatSubsystem과 채팅 창(WarriorChatWidgets)이 부른다.
 */
namespace WarriorChatRules
{
	//입력 칸: 코드 포인트 InMaxLength자까지만 남긴다(서로게이트 쌍을 가르지 않음)
	PROJECTWARRIOR_API FString TruncateToCodePoints(const FString& InText, int32 InMaxLength);

	//글자 수를 코드 포인트로 센다. FString은 UTF-16이라 서로게이트 쌍(이모지 등)을 한 글자로 센다
	PROJECTWARRIOR_API int32 CountCodePoints(const FString& InText);

	//보내기 전 글 규칙: 앞뒤 공백을 자르고(OutTrimmed) 빈 글·길이·제어 문자를 본다. 제어 문자는 서버와 같이 U+0000~001F, U+007F~009F
	PROJECTWARRIOR_API EWarriorChatTextCheck CheckText(const FString& InText, int32 InMaxLength, FString& OutTrimmed);

	//채팅 한 줄 JSON(실시간 data, 응답 data, 내역 messages의 한 칸)을 읽는다. 번호·글·시각이 없거나 scope를 모르면 false
	PROJECTWARRIOR_API bool ParseMessage(const TSharedPtr<FJsonObject>& InObject, FWarriorChatMessage& OutMessage);

	//받은 길드 줄을 그릴지: 내 길드를 알면 같은 길드만. 모르면 받음(서버는 길드원에게만 보낸다)
	PROJECTWARRIOR_API bool IsForMyGuild(const FWarriorChatMessage& InMessage, const FString& InMyGuildId);

	//같은 번호의 줄이 이미 있나(Sending·Failed 줄은 번호가 없어 보지 않는다)
	PROJECTWARRIOR_API bool HasMessage(const TArray<FWarriorChatLine>& InLines, const FString& InMessageId);

	//새 줄을 끝에 붙인다. 같은 번호가 있으면 붙이지 않고 false. InMaxLines를 넘으면 앞(옛것)부터 버린다
	PROJECTWARRIOR_API bool AppendLine(TArray<FWarriorChatLine>& InOutLines, const FWarriorChatLine& InLine, int32 InMaxLines);

	/**
	 * 내역 묶음(새것 → 옛것)을 합친다. 이미 있는 번호는 빼고, 받은 시각 순으로 끼운다(Sending·Failed 줄은 끝에 그대로).
	 * bOlder(위로 더 받기)면 InMaxLines까지 남은 자리만큼 새것부터 넣고 앞을 버리지 않는다. 아니면(첫 묶음·다시 붙음) 다 넣고 넘치면 앞을 버린다.
	 * 넣은 줄 수를 돌려준다
	 */
	PROJECTWARRIOR_API int32 MergeHistory(TArray<FWarriorChatLine>& InOutLines, const TArray<FWarriorChatMessage>& InNewestFirst,
		int32 InMaxLines, bool bOlder);

	/**
	 * 채팅 창 목록을 만든다(chat-api.md "채팅 한 줄" sentAt: PC 시간대로 HH:MM, 날짜가 바뀌는 줄 위에 날짜선).
	 * 날짜선: 앞 줄과 PC 날짜가 다른 줄 위. 첫 줄은 오늘이 아니면 위에 날짜선. InUtcToLocal = FDateTime::Now() − UtcNow()
	 */
	PROJECTWARRIOR_API TArray<FWarriorChatRow> BuildRows(const TArray<FWarriorChatLine>& InLines, FTimespan InUtcToLocal, FDateTime InLocalToday);

	//새 줄이 "안 읽음"인가(U-C4): 남이 쓴 줄이고, 채팅 창이 닫혔거나 다른 탭을 보고 있을 때
	PROJECTWARRIOR_API bool IsUnreadArrival(bool bInPanelOpen, EWarriorChatScope InViewedScope, EWarriorChatScope InArrivedScope, bool bInMine);

	//요일 한 글자(일·월·화·수·목·금·토)
	PROJECTWARRIOR_API FString WeekdayKorean(const FDateTime& InDate);
}
