// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorChatSubsystem.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Chat/WarriorChatRules.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "WarriorChat"

namespace WarriorChat
{
	const FString MessageType = TEXT("CHAT_MESSAGE");
	const FString CodeRateLimited = TEXT("CHAT_RATE_LIMITED");
	const FString CodeMuted = TEXT("CHAT_MUTED");
	const FString CodeGuildNotMember = TEXT("GUILD_NOT_MEMBER");

	const TCHAR* ScopeName(EWarriorChatScope InScope)
	{
		return InScope == EWarriorChatScope::Guild ? TEXT("guild") : TEXT("global");
	}

	//성공 응답 본문에서 data 객체를 꺼낸다
	TSharedPtr<FJsonObject> ReadData(const FString& InBody)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InBody);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Data = nullptr;
		return Root->TryGetObjectField(TEXT("data"), Data) && Data ? *Data : nullptr;
	}
}

UWarriorChatSubsystem* UWarriorChatSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorChatSubsystem>() : nullptr;
}

void UWarriorChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//보내기는 Api, 세션은 Auth, 받기는 Realtime. 먼저 있어야 알림에 붙을 수 있다
	Collection.InitializeDependency<UWarriorApiSubsystem>();
	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
	if (UWarriorRealtimeSubsystem* Realtime = Collection.InitializeDependency<UWarriorRealtimeSubsystem>())
	{
		//OnMessage는 동적 델리게이트가 아니다(C++ 전용) → AddUObject, Deinitialize에서 뗀다
		RealtimeMessageHandle = Realtime->OnMessage.AddUObject(this, &ThisClass::HandleRealtimeMessage);
		Realtime->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRealtimeStateChanged);
	}
}

void UWarriorChatSubsystem::Deinitialize()
{
	StopUnblockTicker();

	if (UWarriorRealtimeSubsystem* Realtime = GetGameInstance()->GetSubsystem<UWarriorRealtimeSubsystem>())
	{
		Realtime->OnMessage.Remove(RealtimeMessageHandle);
		Realtime->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleRealtimeStateChanged);
	}
	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}
	RealtimeMessageHandle.Reset();

	Super::Deinitialize();
}

EWarriorChatSendResult UWarriorChatSubsystem::SendMessage(EWarriorChatScope Scope, const FString& Text)
{
	const UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>();
	if (!Auth || !Auth->IsLoggedIn())
	{
		return EWarriorChatSendResult::NotLoggedIn;
	}

	FString Trimmed;
	switch (WarriorChatRules::CheckText(Text, MaxTextLength, Trimmed))
	{
	case EWarriorChatTextCheck::Empty:
		return EWarriorChatSendResult::Empty;
	case EWarriorChatTextCheck::TooLong:
		return EWarriorChatSendResult::TooLong;
	case EWarriorChatTextCheck::ControlChar:
		return EWarriorChatSendResult::ControlChar;
	default:
		break;
	}
	if (GetSendBlockedSeconds() > 0)
	{
		return EWarriorChatSendResult::Blocked;
	}

	//내 줄을 "보내는 중"으로 먼저 붙인다. 번호·시각은 응답이 오면 서버 값으로 바뀐다
	FWarriorChatLine Line;
	Line.State = EWarriorChatLineState::Sending;
	Line.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Line.Message.Scope = Scope;
	Line.Message.GuildId = Scope == EWarriorChatScope::Guild ? MyGuildId : FString();
	Line.Message.SenderAccountId = Auth->GetAccount().AccountId;
	Line.Message.SenderNickname = Auth->GetAccount().Nickname;
	Line.Message.Text = Trimmed;
	Line.Message.SentAt = FDateTime::UtcNow();
	WarriorChatRules::AppendLine(LinesOf(Scope), Line, MaxLinesPerTab);

	const bool bStarted = SendLine(Scope, Line.RequestId, Trimmed);
	if (!bStarted)
	{
		const int32 Index = FindPendingLine(LinesOf(Scope), Line.RequestId);
		if (Index != INDEX_NONE)
		{
			LinesOf(Scope)[Index].State = EWarriorChatLineState::Failed;
		}
	}
	OnChatUpdated.Broadcast(Scope);
	return bStarted ? EWarriorChatSendResult::Sending : EWarriorChatSendResult::NotSent;
}

bool UWarriorChatSubsystem::RetryLine(EWarriorChatScope Scope, const FString& RequestId)
{
	TArray<FWarriorChatLine>& Lines = LinesOf(Scope);
	const int32 Index = FindPendingLine(Lines, RequestId);
	if (Index == INDEX_NONE || Lines[Index].State != EWarriorChatLineState::Failed || GetSendBlockedSeconds() > 0)
	{
		return false;
	}
	//같은 requestId·같은 글: 서버가 이미 받았으면 처음 응답(같은 messageId)이 온다(chat-api.md "한 번만 반영")
	Lines[Index].State = EWarriorChatLineState::Sending;
	const FString Text = Lines[Index].Message.Text;
	if (!SendLine(Scope, RequestId, Text))
	{
		Lines[Index].State = EWarriorChatLineState::Failed;
		OnChatUpdated.Broadcast(Scope);
		return false;
	}
	OnChatUpdated.Broadcast(Scope);
	return true;
}

void UWarriorChatSubsystem::DiscardLine(EWarriorChatScope Scope, const FString& RequestId)
{
	TArray<FWarriorChatLine>& Lines = LinesOf(Scope);
	const int32 Index = FindPendingLine(Lines, RequestId);
	if (Index != INDEX_NONE && Lines[Index].State == EWarriorChatLineState::Failed)
	{
		Lines.RemoveAt(Index);
		OnChatUpdated.Broadcast(Scope);
	}
}

bool UWarriorChatSubsystem::LoadGuildHistory(bool bOlder)
{
	const UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>();
	UWarriorApiSubsystem* Api = GetGameInstance()->GetSubsystem<UWarriorApiSubsystem>();
	if (!Auth || !Auth->IsLoggedIn() || !Api || bLoadingGuildHistory)
	{
		return false;
	}
	if (bOlder && !HasOlderGuildHistory())
	{
		return false;
	}

	FWarriorApiRequest Request;
	Request.Verb = TEXT("GET");
	Request.Path = FString::Printf(TEXT("/chats/guild?size=%d"), FMath::Clamp(HistoryPageSize, 1, 50));
	if (bOlder)
	{
		Request.Path += TEXT("&before=") + FGenericPlatformHttp::UrlEncode(GuildHistoryCursor);
	}
	Request.Context = WarriorErrorContext::Chat;

	const int32 SentSessionSerial = CurrentSessionSerial();
	const int32 SentGuildTabSerial = GuildTabSerial;
	bLoadingGuildHistory = Api->Send(Request,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), bOlder, SentSessionSerial, SentGuildTabSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleHistoryResult(bOlder, SentSessionSerial, SentGuildTabSerial, Result);
			}
		});
	if (!bLoadingGuildHistory)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Chat] Guild history request not sent."));
	}
	return bLoadingGuildHistory;
}

TArray<FWarriorChatLine> UWarriorChatSubsystem::GetLines(EWarriorChatScope Scope) const
{
	return LinesOf(Scope);
}

bool UWarriorChatSubsystem::HasOlderGuildHistory() const
{
	return !GuildHistoryCursor.IsEmpty() && GuildLines.Num() < MaxLinesPerTab;
}

int32 UWarriorChatSubsystem::GetSendBlockedSeconds() const
{
	const double Remaining = SendBlockedUntil - FPlatformTime::Seconds();
	return Remaining > 0.0 ? FMath::CeilToInt32(Remaining) : 0;
}

void UWarriorChatSubsystem::SetMyGuildId(const FString& InGuildId)
{
	const EWarriorChatGuildState NewState = InGuildId.IsEmpty() ? EWarriorChatGuildState::None : EWarriorChatGuildState::Member;
	if (InGuildId == MyGuildId && NewState == GuildState)
	{
		return;
	}
	UE_LOG(LogProjectWarrior, Log, TEXT("[Chat] My guild changed. guildId=%s"), InGuildId.IsEmpty() ? TEXT("(none)") : *InGuildId);
	MyGuildId = InGuildId;
	ResetGuildTab();
	SetGuildState(NewState);
	OnChatUpdated.Broadcast(EWarriorChatScope::Guild);
	if (NewState == EWarriorChatGuildState::Member)
	{
		LoadGuildHistory(false);
	}
}

void UWarriorChatSubsystem::HandleRealtimeMessage(const FString& InType, const FString& InId, const TSharedPtr<FJsonObject>& InData)
{
	if (InType != WarriorChat::MessageType)
	{
		return;
	}
	FWarriorChatMessage Message;
	if (!WarriorChatRules::ParseMessage(InData, Message))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Chat] CHAT_MESSAGE data could not be read. id=%s"), *InId);
		return;
	}
	if (Message.Scope == EWarriorChatScope::Guild)
	{
		if (!WarriorChatRules::IsForMyGuild(Message, MyGuildId))
		{
			UE_LOG(LogProjectWarrior, Verbose, TEXT("[Chat] Guild message for another guild dropped. guildId=%s mine=%s"), *Message.GuildId, *MyGuildId);
			return;
		}
		//서버는 길드원에게만 보낸다: 받았으면 그 길드원이다(U2 전에는 이렇게 안다)
		AdoptGuildId(Message.GuildId);
	}

	FWarriorChatLine Line;
	Line.Message = Message;
	if (WarriorChatRules::AppendLine(LinesOf(Message.Scope), Line, MaxLinesPerTab))
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Chat] Received %s messageId=%s from=%s"),
			WarriorChat::ScopeName(Message.Scope), *Message.MessageId, *Message.SenderAccountId);
		OnChatUpdated.Broadcast(Message.Scope);
	}
}

void UWarriorChatSubsystem::HandleRealtimeStateChanged(EWarriorRealtimeState InState)
{
	//붙을 때마다(로그인 뒤 첫 연결·다시 붙음) 놓친 길드 채팅을 첫 묶음으로 채운다. 길드 없음이면 묻지 않는다
	if (InState == EWarriorRealtimeState::Connected && GuildState != EWarriorChatGuildState::None)
	{
		LoadGuildHistory(false);
	}
}

void UWarriorChatSubsystem::HandleSessionEnded(EWarriorSessionEndReason Reason, const FText& Message)
{
	ClearAll();
}

bool UWarriorChatSubsystem::SendLine(EWarriorChatScope InScope, const FString& InRequestId, const FString& InText)
{
	UWarriorApiSubsystem* Api = GetGameInstance()->GetSubsystem<UWarriorApiSubsystem>();
	if (!Api)
	{
		return false;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("requestId"), InRequestId);
	Body->SetStringField(TEXT("text"), InText);
	FString BodyText;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&BodyText);
	if (!FJsonSerializer::Serialize(Body, Writer))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Chat] Send body could not be built. requestId=%s"), *InRequestId);
		return false;
	}

	FWarriorApiRequest Request;
	Request.Verb = TEXT("POST");
	Request.Path = InScope == EWarriorChatScope::Guild ? TEXT("/chats/guild") : TEXT("/chats/global");
	Request.Body = BodyText;
	Request.RequestId = InRequestId;
	Request.StoreKind = EWarriorPendingKind::None;
	Request.bAskOnFail = false;
	Request.Context = WarriorErrorContext::Chat;

	const int32 SentSessionSerial = CurrentSessionSerial();
	const bool bStarted = Api->Send(Request,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InScope, InRequestId, SentSessionSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleSendResult(InScope, InRequestId, SentSessionSerial, Result);
			}
		});
	UE_LOG(LogProjectWarrior, Log, TEXT("[Chat] Send %s %s. requestId=%s"),
		WarriorChat::ScopeName(InScope), bStarted ? TEXT("started") : TEXT("not started"), *InRequestId);
	return bStarted;
}

void UWarriorChatSubsystem::HandleSendResult(EWarriorChatScope InScope, const FString& InRequestId, int32 InSessionSerial,
	const FWarriorApiResult& InResult)
{
	if (InSessionSerial != CurrentSessionSerial())
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Chat] Send result of an old session ignored. requestId=%s"), *InRequestId);
		return;
	}
	TArray<FWarriorChatLine>& Lines = LinesOf(InScope);
	const int32 Index = FindPendingLine(Lines, InRequestId);
	if (Index == INDEX_NONE)
	{
		//길드가 바뀌어 길드 탭을 비웠거나 50줄에서 밀려남
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Chat] Send result for a line no longer shown. requestId=%s"), *InRequestId);
		return;
	}

	if (InResult.bOk)
	{
		FWarriorChatMessage Message;
		if (!WarriorChatRules::ParseMessage(WarriorChat::ReadData(InResult.Body), Message))
		{
			//서버는 받았다. [다시]를 누르면 같은 requestId라 처음 응답이 다시 온다
			UE_LOG(LogProjectWarrior, Error, TEXT("[Chat] Send response could not be read. requestId=%s"), *InRequestId);
			Lines[Index].State = EWarriorChatLineState::Failed;
			OnChatUpdated.Broadcast(InScope);
			return;
		}
		if (WarriorChatRules::HasMessage(Lines, Message.MessageId))
		{
			//실시간 사본이 먼저 왔다: 내 임시 줄만 지운다
			Lines.RemoveAt(Index);
		}
		else
		{
			Lines[Index].Message = Message;
			Lines[Index].State = EWarriorChatLineState::Sent;
		}
		UE_LOG(LogProjectWarrior, Log, TEXT("[Chat] Sent %s messageId=%s requestId=%s"),
			WarriorChat::ScopeName(InScope), *Message.MessageId, *InRequestId);
		if (InScope == EWarriorChatScope::Guild)
		{
			AdoptGuildId(Message.GuildId);
		}
		OnChatUpdated.Broadcast(InScope);
		return;
	}

	const FString& Code = InResult.Error.Code;
	const int32 Status = InResult.HttpStatus;
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Chat] Send %s failed. status=%d code=%s requestId=%s"),
		WarriorChat::ScopeName(InScope), Status, *Code, *InRequestId);

	if (Status == 400 || Status == 403 || Status == 429)
	{
		//서버가 받지 않은 글: 줄을 지우고 글을 입력 칸으로 돌려준다(429는 자동으로 다시 보내지 않음)
		const FString Text = Lines[Index].Message.Text;
		Lines.RemoveAt(Index);
		FText Notice = InResult.Rule.Text;
		if (Status == 429)
		{
			const int32 Seconds = FMath::Max(1, InResult.Error.RetryAfterSeconds);
			BlockSending(Code, Seconds);
			const FWarriorErrorRule* Rule = FWarriorErrorText::Find(Code);
			Notice = FText::FormatNamed(Rule ? Rule->Text : InResult.Rule.Text, TEXT("Seconds"), FText::AsNumber(Seconds));
		}
		if (Code == WarriorChat::CodeGuildNotMember)
		{
			ApplyGuildNotMember();
		}
		OnChatUpdated.Broadcast(InScope);
		OnSendRejected.Broadcast(InScope, Code, Notice, Text);
		return;
	}

	//응답 없음·5xx(자동 재시도를 다 씀)·409 등: "보내지 못함 [다시]"
	Lines[Index].State = EWarriorChatLineState::Failed;
	OnChatUpdated.Broadcast(InScope);
}

void UWarriorChatSubsystem::HandleHistoryResult(bool bOlder, int32 InSessionSerial, int32 InGuildTabSerial,
	const FWarriorApiResult& InResult)
{
	if (InSessionSerial != CurrentSessionSerial() || InGuildTabSerial != GuildTabSerial)
	{
		//지난 세션이거나 그사이 길드 탭을 비웠다(길드가 바뀜). 새 요청이 bLoadingGuildHistory를 맡는다
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Chat] Old guild history response ignored."));
		return;
	}
	bLoadingGuildHistory = false;

	if (!InResult.bOk)
	{
		if (InResult.Error.Code == WarriorChat::CodeGuildNotMember)
		{
			ApplyGuildNotMember();
		}
		else
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Chat] Guild history failed. status=%d code=%s"),
				InResult.HttpStatus, *InResult.Error.Code);
		}
		OnChatUpdated.Broadcast(EWarriorChatScope::Guild);
		return;
	}

	const TSharedPtr<FJsonObject> Data = WarriorChat::ReadData(InResult.Body);
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Data.IsValid() || !Data->TryGetArrayField(TEXT("messages"), Items) || !Items)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Chat] Guild history response could not be read."));
		OnChatUpdated.Broadcast(EWarriorChatScope::Guild);
		return;
	}
	TArray<FWarriorChatMessage> NewestFirst;
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		FWarriorChatMessage Message;
		if (Item.IsValid() && WarriorChatRules::ParseMessage(Item->AsObject(), Message) && Message.Scope == EWarriorChatScope::Guild)
		{
			NewestFirst.Add(MoveTemp(Message));
		}
	}
	FString NextCursor;
	Data->TryGetStringField(TEXT("nextCursor"), NextCursor);

	//서버가 내 길드로 정해 준 내역이다
	if (!NewestFirst.IsEmpty())
	{
		AdoptGuildId(NewestFirst[0].GuildId);
	}
	SetGuildState(EWarriorChatGuildState::Member);

	//커서: 위로 더 받기면 늘 새 커서. 첫 묶음이면 가진 줄이 없을 때만(다시 붙어 채울 때는 이미 더 옛 커서를 가짐)
	const bool bHadLines = GuildLines.ContainsByPredicate([](const FWarriorChatLine& Line) { return Line.State == EWarriorChatLineState::Sent; });
	if (bOlder || !bHadLines)
	{
		GuildHistoryCursor = NextCursor;
	}
	const int32 Added = WarriorChatRules::MergeHistory(GuildLines, NewestFirst, MaxLinesPerTab, bOlder);
	UE_LOG(LogProjectWarrior, Log, TEXT("[Chat] Guild history %s. received=%d added=%d more=%s"),
		bOlder ? TEXT("older") : TEXT("latest"), NewestFirst.Num(), Added, HasOlderGuildHistory() ? TEXT("yes") : TEXT("no"));
	OnChatUpdated.Broadcast(EWarriorChatScope::Guild);
}

void UWarriorChatSubsystem::ApplyGuildNotMember()
{
	const bool bChanged = GuildState != EWarriorChatGuildState::None || !GuildLines.IsEmpty();
	MyGuildId.Reset();
	ResetGuildTab();
	SetGuildState(EWarriorChatGuildState::None);
	if (bChanged)
	{
		OnChatUpdated.Broadcast(EWarriorChatScope::Guild);
	}
}

void UWarriorChatSubsystem::AdoptGuildId(const FString& InGuildId)
{
	if (InGuildId.IsEmpty() || InGuildId == MyGuildId)
	{
		SetGuildState(EWarriorChatGuildState::Member);
		return;
	}
	if (!MyGuildId.IsEmpty())
	{
		//서버 응답이 다른 길드를 말함: 길드가 바뀐 것으로 보고 길드 탭을 새로
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Chat] Server says another guild. mine=%s server=%s"), *MyGuildId, *InGuildId);
		SetMyGuildId(InGuildId);
		return;
	}
	MyGuildId = InGuildId;
	SetGuildState(EWarriorChatGuildState::Member);
}

void UWarriorChatSubsystem::SetGuildState(EWarriorChatGuildState InState)
{
	if (GuildState != InState)
	{
		GuildState = InState;
		OnGuildStateChanged.Broadcast(GuildState);
	}
}

void UWarriorChatSubsystem::ResetGuildTab()
{
	GuildLines.Reset();
	GuildHistoryCursor.Reset();
	//받는 중이던 내역은 옛 탭 것이다: 응답을 버리고 새로 받을 수 있게 한다
	++GuildTabSerial;
	bLoadingGuildHistory = false;
}

void UWarriorChatSubsystem::BlockSending(const FString& InCode, int32 InSeconds)
{
	SendBlockedUntil = FPlatformTime::Seconds() + InSeconds;
	StopUnblockTicker();
	UnblockTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleUnblockTick), static_cast<float>(InSeconds));
	UE_LOG(LogProjectWarrior, Log, TEXT("[Chat] Sending blocked for %d s. code=%s"), InSeconds, *InCode);
	const FWarriorErrorRule* Rule = FWarriorErrorText::Find(InCode);
	OnSendStateChanged.Broadcast(true,
		FText::FormatNamed(Rule ? Rule->Text : FWarriorErrorText::GetText(InCode, WarriorErrorContext::Chat), TEXT("Seconds"), FText::AsNumber(InSeconds)));
}

bool UWarriorChatSubsystem::HandleUnblockTick(float InDeltaTime)
{
	UnblockTickerHandle.Reset();
	if (GetSendBlockedSeconds() > 0)
	{
		//다시 막혔다(새 429가 늦게 온 경우): 새 티커가 이미 걸려 있다
		return false;
	}
	OnSendStateChanged.Broadcast(false, FText::GetEmpty());
	return false;
}

void UWarriorChatSubsystem::StopUnblockTicker()
{
	if (UnblockTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(UnblockTickerHandle);
		UnblockTickerHandle.Reset();
	}
}

void UWarriorChatSubsystem::ClearAll()
{
	StopUnblockTicker();
	const bool bWasBlocked = GetSendBlockedSeconds() > 0;
	SendBlockedUntil = 0.0;
	GlobalLines.Reset();
	MyGuildId.Reset();
	ResetGuildTab();
	SetGuildState(EWarriorChatGuildState::Unknown);
	OnChatUpdated.Broadcast(EWarriorChatScope::Global);
	OnChatUpdated.Broadcast(EWarriorChatScope::Guild);
	if (bWasBlocked)
	{
		OnSendStateChanged.Broadcast(false, FText::GetEmpty());
	}
}

int32 UWarriorChatSubsystem::FindPendingLine(const TArray<FWarriorChatLine>& InLines, const FString& InRequestId)
{
	if (InRequestId.IsEmpty())
	{
		return INDEX_NONE;
	}
	return InLines.IndexOfByPredicate([&InRequestId](const FWarriorChatLine& Line)
	{
		return Line.State != EWarriorChatLineState::Sent && Line.RequestId == InRequestId;
	});
}

int32 UWarriorChatSubsystem::CurrentSessionSerial() const
{
	const UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>();
	return Auth ? Auth->GetSessionSerial() : 0;
}

#undef LOCTEXT_NAMESPACE
