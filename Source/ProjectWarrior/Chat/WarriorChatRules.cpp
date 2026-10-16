// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorChatRules.h"

#include "Dom/JsonObject.h"

namespace WarriorChatRules
{
	namespace
	{
		bool IsHighSurrogate(TCHAR C) { return C >= 0xD800 && C <= 0xDBFF; }
		bool IsLowSurrogate(TCHAR C) { return C >= 0xDC00 && C <= 0xDFFF; }

		//Java Character.isISOControl과 같다(서버 ChatTextPolicy)
		bool IsIsoControl(uint32 CodePoint)
		{
			return CodePoint <= 0x1F || (CodePoint >= 0x7F && CodePoint <= 0x9F);
		}

		bool ParseScope(const FString& InScope, EWarriorChatScope& OutScope)
		{
			if (InScope == TEXT("GLOBAL"))
			{
				OutScope = EWarriorChatScope::Global;
				return true;
			}
			if (InScope == TEXT("GUILD"))
			{
				OutScope = EWarriorChatScope::Guild;
				return true;
			}
			return false;
		}
	}

	FString TruncateToCodePoints(const FString& InText, int32 InMaxLength)
	{
		int32 Count = 0;
		const int32 Len = InText.Len();
		for (int32 Index = 0; Index < Len; ++Index)
		{
			if (Count == InMaxLength)
			{
				return InText.Left(Index);
			}
			if (IsHighSurrogate(InText[Index]) && Index + 1 < Len && IsLowSurrogate(InText[Index + 1]))
			{
				++Index;
			}
			++Count;
		}
		return InText;
	}

	int32 CountCodePoints(const FString& InText)
	{
		int32 Count = 0;
		const int32 Len = InText.Len();
		for (int32 Index = 0; Index < Len; ++Index)
		{
			if (IsHighSurrogate(InText[Index]) && Index + 1 < Len && IsLowSurrogate(InText[Index + 1]))
			{
				++Index;
			}
			++Count;
		}
		return Count;
	}

	EWarriorChatTextCheck CheckText(const FString& InText, int32 InMaxLength, FString& OutTrimmed)
	{
		OutTrimmed = InText.TrimStartAndEnd();
		if (OutTrimmed.IsEmpty())
		{
			return EWarriorChatTextCheck::Empty;
		}
		for (const TCHAR C : OutTrimmed)
		{
			//서로게이트는 제어 문자가 아니므로 UTF-16 단위로 봐도 된다
			if (IsIsoControl(static_cast<uint32>(C)))
			{
				return EWarriorChatTextCheck::ControlChar;
			}
		}
		if (CountCodePoints(OutTrimmed) > InMaxLength)
		{
			return EWarriorChatTextCheck::TooLong;
		}
		return EWarriorChatTextCheck::Ok;
	}

	bool ParseMessage(const TSharedPtr<FJsonObject>& InObject, FWarriorChatMessage& OutMessage)
	{
		if (!InObject.IsValid())
		{
			return false;
		}
		FString ScopeText;
		FString SentAtText;
		FWarriorChatMessage Message;
		if (!InObject->TryGetStringField(TEXT("messageId"), Message.MessageId) || Message.MessageId.IsEmpty()
			|| !InObject->TryGetStringField(TEXT("scope"), ScopeText) || !ParseScope(ScopeText, Message.Scope)
			|| !InObject->TryGetStringField(TEXT("text"), Message.Text)
			|| !InObject->TryGetStringField(TEXT("sentAt"), SentAtText) || !FDateTime::ParseIso8601(*SentAtText, Message.SentAt))
		{
			return false;
		}
		//guildId는 전체 채팅이면 null(TryGetStringField가 false → 빈 값)
		InObject->TryGetStringField(TEXT("guildId"), Message.GuildId);
		if (Message.Scope == EWarriorChatScope::Guild && Message.GuildId.IsEmpty())
		{
			return false;
		}
		InObject->TryGetStringField(TEXT("senderAccountId"), Message.SenderAccountId);
		InObject->TryGetStringField(TEXT("senderNickname"), Message.SenderNickname);
		OutMessage = MoveTemp(Message);
		return true;
	}

	bool IsForMyGuild(const FWarriorChatMessage& InMessage, const FString& InMyGuildId)
	{
		return InMessage.Scope == EWarriorChatScope::Guild && (InMyGuildId.IsEmpty() || InMessage.GuildId == InMyGuildId);
	}

	bool HasMessage(const TArray<FWarriorChatLine>& InLines, const FString& InMessageId)
	{
		return !InMessageId.IsEmpty() && InLines.ContainsByPredicate([&InMessageId](const FWarriorChatLine& Line)
		{
			return Line.Message.MessageId == InMessageId;
		});
	}

	bool AppendLine(TArray<FWarriorChatLine>& InOutLines, const FWarriorChatLine& InLine, int32 InMaxLines)
	{
		if (HasMessage(InOutLines, InLine.Message.MessageId))
		{
			return false;
		}
		InOutLines.Add(InLine);
		const int32 Over = InOutLines.Num() - FMath::Max(1, InMaxLines);
		if (Over > 0)
		{
			InOutLines.RemoveAt(0, Over);
		}
		return true;
	}

	int32 MergeHistory(TArray<FWarriorChatLine>& InOutLines, const TArray<FWarriorChatMessage>& InNewestFirst,
		int32 InMaxLines, bool bOlder)
	{
		const int32 MaxLines = FMath::Max(1, InMaxLines);

		//새것 → 옛것 순으로 보며 없는 줄만 고른다(위로 더 받기면 남은 자리까지)
		int32 Room = bOlder ? MaxLines - InOutLines.Num() : MAX_int32;
		TArray<FWarriorChatLine> Added;
		for (const FWarriorChatMessage& Message : InNewestFirst)
		{
			if (Room <= 0)
			{
				break;
			}
			if (HasMessage(InOutLines, Message.MessageId)
				|| Added.ContainsByPredicate([&Message](const FWarriorChatLine& Line) { return Line.Message.MessageId == Message.MessageId; }))
			{
				continue;
			}
			FWarriorChatLine Line;
			Line.Message = Message;
			Added.Add(MoveTemp(Line));
			--Room;
		}
		if (Added.IsEmpty())
		{
			return 0;
		}

		//받은 줄(Sent)만 시각 순으로 다시 세우고, 보내는 중·실패 줄은 끝에 그대로 둔다
		const int32 AddedCount = Added.Num();
		TArray<FWarriorChatLine> Sent;
		TArray<FWarriorChatLine> Pending;
		for (FWarriorChatLine& Line : InOutLines)
		{
			(Line.State == EWarriorChatLineState::Sent ? Sent : Pending).Add(MoveTemp(Line));
		}
		Sent.Append(MoveTemp(Added));
		Sent.StableSort([](const FWarriorChatLine& A, const FWarriorChatLine& B) { return A.Message.SentAt < B.Message.SentAt; });

		InOutLines = MoveTemp(Sent);
		InOutLines.Append(MoveTemp(Pending));
		const int32 Over = InOutLines.Num() - MaxLines;
		if (Over > 0)
		{
			InOutLines.RemoveAt(0, Over);
		}
		return AddedCount;
	}

	TArray<FWarriorChatRow> BuildRows(const TArray<FWarriorChatLine>& InLines, FTimespan InUtcToLocal, FDateTime InLocalToday)
	{
		TArray<FWarriorChatRow> Rows;
		Rows.Reserve(InLines.Num() + 2);
		FDateTime PreviousDate;
		for (int32 Index = 0; Index < InLines.Num(); ++Index)
		{
			const FDateTime LocalDate = (InLines[Index].Message.SentAt + InUtcToLocal).GetDate();
			const bool bNewDate = Index == 0 ? LocalDate != InLocalToday.GetDate() : LocalDate != PreviousDate;
			if (bNewDate)
			{
				FWarriorChatRow DateRow;
				DateRow.bDateLine = true;
				DateRow.LocalDate = LocalDate;
				Rows.Add(DateRow);
			}
			FWarriorChatRow LineRow;
			LineRow.LineIndex = Index;
			Rows.Add(LineRow);
			PreviousDate = LocalDate;
		}
		return Rows;
	}

	FString WeekdayKorean(const FDateTime& InDate)
	{
		//EDayOfWeek는 월요일이 0
		static const TCHAR* const Names[] = { TEXT("월"), TEXT("화"), TEXT("수"), TEXT("목"), TEXT("금"), TEXT("토"), TEXT("일") };
		return Names[static_cast<int32>(InDate.GetDayOfWeek())];
	}
}
