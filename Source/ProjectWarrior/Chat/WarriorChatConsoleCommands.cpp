#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Chat/WarriorChatSubsystem.h"

/**
 * 채팅(M23) 시험용 콘솔 명령. 채팅 창 위젯(U-C2·U-C3) 전에 PIE에서 쓴다.
 *   Warrior.Chat.Send global 안녕하세요     전체 채팅 보내기(글은 띄어쓰기 포함 나머지 전부)
 *   Warrior.Chat.Send guild 길드 안녕       길드 채팅 보내기
 *   Warrior.Chat.History                    길드 내역 최신 묶음 / Warrior.Chat.History older  이전 묶음
 *   Warrior.Chat.Dump                       두 탭의 줄·길드 상태·막힘 남은 초를 로그로
 * 로그는 LogProjectWarrior "[Chat]". Dump만 글을 로그에 남긴다(시험용)
 */
namespace WarriorChatConsole
{
	const TCHAR* StateName(EWarriorChatLineState InState)
	{
		switch (InState)
		{
		case EWarriorChatLineState::Sending: return TEXT("sending");
		case EWarriorChatLineState::Failed: return TEXT("FAILED");
		default: return TEXT("sent");
		}
	}

	void Send(const TArray<FString>& Args, UWorld* World)
	{
		UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(World);
		if (!Chat || Args.Num() < 2)
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Chat] Usage: Warrior.Chat.Send <global|guild> <text>"));
			return;
		}
		const EWarriorChatScope Scope = Args[0].Equals(TEXT("guild"), ESearchCase::IgnoreCase)
			? EWarriorChatScope::Guild : EWarriorChatScope::Global;
		const FString Text = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		const EWarriorChatSendResult Result = Chat->SendMessage(Scope, Text);
		UE_LOG(LogProjectWarrior, Display, TEXT("[Chat] Send command result=%s"), *UEnum::GetValueAsString(Result));
	}

	void History(const TArray<FString>& Args, UWorld* World)
	{
		UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(World);
		if (!Chat)
		{
			return;
		}
		const bool bOlder = Args.Num() > 0 && Args[0].Equals(TEXT("older"), ESearchCase::IgnoreCase);
		UE_LOG(LogProjectWarrior, Display, TEXT("[Chat] History command %s started=%s"),
			bOlder ? TEXT("older") : TEXT("latest"), Chat->LoadGuildHistory(bOlder) ? TEXT("yes") : TEXT("no"));
	}

	void Dump(const TArray<FString>& Args, UWorld* World)
	{
		const UWarriorChatSubsystem* Chat = UWarriorChatSubsystem::Get(World);
		if (!Chat)
		{
			return;
		}
		UE_LOG(LogProjectWarrior, Display, TEXT("[Chat] guildState=%s guildId=%s blocked=%ds moreHistory=%s"),
			*UEnum::GetValueAsString(Chat->GetGuildState()), *Chat->GetMyGuildId(), Chat->GetSendBlockedSeconds(),
			Chat->HasOlderGuildHistory() ? TEXT("yes") : TEXT("no"));
		for (const EWarriorChatScope Scope : { EWarriorChatScope::Global, EWarriorChatScope::Guild })
		{
			const TArray<FWarriorChatLine> Lines = Chat->GetLines(Scope);
			UE_LOG(LogProjectWarrior, Display, TEXT("[Chat] -- %s (%d lines)"), *UEnum::GetValueAsString(Scope), Lines.Num());
			for (const FWarriorChatLine& Line : Lines)
			{
				UE_LOG(LogProjectWarrior, Display, TEXT("[Chat]   [%s] %s %s(%s): %s  id=%s"),
					StateName(Line.State), *Line.Message.SentAt.ToString(TEXT("%m-%d %H:%M:%S")), *Line.Message.SenderNickname,
					*Line.Message.SenderAccountId, *Line.Message.Text, *Line.Message.MessageId);
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs SendCommand(
		TEXT("Warrior.Chat.Send"), TEXT("채팅을 보낸다: Warrior.Chat.Send <global|guild> <글> (디버그용, M23)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Send));

	FAutoConsoleCommandWithWorldAndArgs HistoryCommand(
		TEXT("Warrior.Chat.History"), TEXT("길드 채팅 내역을 받는다. older면 이전 묶음 (디버그용, M23)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&History));

	FAutoConsoleCommandWithWorldAndArgs DumpCommand(
		TEXT("Warrior.Chat.Dump"), TEXT("채팅 두 탭의 줄과 상태를 로그로 (디버그용, M23)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
