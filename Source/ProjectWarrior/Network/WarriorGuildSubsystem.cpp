// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGuildSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Network/WarriorApiSubsystem.h"
#include "ProjectWarrior/Network/WarriorErrorText.h"

//길드 응답 읽기. 명세 guild-api.md v1의 칸 이름 그대로 읽는다(null·없는 칸은 빈 값)
namespace WarriorGuildApi
{
	const TCHAR* MyGuildPath = TEXT("/accounts/me/guild");
	const TCHAR* GuildsPath = TEXT("/guilds");
	const TCHAR* MyJoinRequestsPath = TEXT("/accounts/me/guild-join-requests");
	const TCHAR* ReceivedJoinRequestsPath = TEXT("/accounts/me/guild/join-requests");
	const TCHAR* LeavePath = TEXT("/accounts/me/guild/leave");
	const TCHAR* DisbandPath = TEXT("/accounts/me/guild/disband");

	//성공 응답 { "data": {...}, "meta": {...} }의 data. 못 읽으면 nullptr
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

	//칸이 없거나 null이면 nullptr
	TSharedPtr<FJsonObject> GetObjectOrNull(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		const TSharedPtr<FJsonObject>* Value = nullptr;
		return InObject.IsValid() && InObject->TryGetObjectField(InField, Value) && Value ? *Value : nullptr;
	}

	FString GetStringOrEmpty(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		FString Value;
		return InObject->TryGetStringField(InField, Value) ? Value : FString();
	}

	int32 GetIntOrZero(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		double Value = 0.0;
		return InObject->TryGetNumberField(InField, Value) ? static_cast<int32>(Value) : 0;
	}

	bool GetBoolOrFalse(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		bool bValue = false;
		return InObject->TryGetBoolField(InField, bValue) && bValue;
	}

	FDateTime GetTime(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		FDateTime Time;
		return FDateTime::ParseIso8601(*GetStringOrEmpty(InObject, InField), Time) ? Time : FDateTime();
	}

	//LEADER·MEMBER. 모르는 값은 None
	EWarriorGuildRole ReadRole(const FString& InRole)
	{
		if (InRole == TEXT("LEADER"))
		{
			return EWarriorGuildRole::Leader;
		}
		return InRole == TEXT("MEMBER") ? EWarriorGuildRole::Member : EWarriorGuildRole::None;
	}

	//배열 칸의 객체마다 InRead. 칸이 없거나 null이면 빈 배열
	template <typename T, typename FRead>
	TArray<T> ReadArray(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField, FRead InRead)
	{
		TArray<T> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!InObject->TryGetArrayField(InField, Values) || !Values)
		{
			return Result;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object)
			{
				Result.Add(InRead(*Object));
			}
		}
		return Result;
	}

	FWarriorGuildSummary ReadSummary(const TSharedPtr<FJsonObject>& InObject)
	{
		FWarriorGuildSummary Summary;
		Summary.GuildId = GetStringOrEmpty(InObject, TEXT("guildId"));
		Summary.Name = GetStringOrEmpty(InObject, TEXT("name"));
		Summary.LeaderAccountId = GetStringOrEmpty(InObject, TEXT("leaderAccountId"));
		Summary.LeaderNickname = GetStringOrEmpty(InObject, TEXT("leaderNickname"));
		Summary.MemberCount = GetIntOrZero(InObject, TEXT("memberCount"));
		Summary.MaxMembers = GetIntOrZero(InObject, TEXT("maxMembers"));
		Summary.CreatedAt = GetTime(InObject, TEXT("createdAt"));
		return Summary;
	}

	FWarriorGuildMember ReadMember(const TSharedPtr<FJsonObject>& InObject)
	{
		FWarriorGuildMember Member;
		Member.AccountId = GetStringOrEmpty(InObject, TEXT("accountId"));
		Member.Nickname = GetStringOrEmpty(InObject, TEXT("nickname"));
		Member.Role = ReadRole(GetStringOrEmpty(InObject, TEXT("role")));
		Member.JoinedAt = GetTime(InObject, TEXT("joinedAt"));
		Member.bOnline = GetBoolOrFalse(InObject, TEXT("online"));
		return Member;
	}

	FWarriorGuildJoinRequest ReadJoinRequest(const TSharedPtr<FJsonObject>& InObject)
	{
		FWarriorGuildJoinRequest Request;
		Request.JoinRequestId = GetStringOrEmpty(InObject, TEXT("joinRequestId"));
		Request.GuildId = GetStringOrEmpty(InObject, TEXT("guildId"));
		Request.GuildName = GetStringOrEmpty(InObject, TEXT("guildName"));
		Request.AccountId = GetStringOrEmpty(InObject, TEXT("accountId"));
		Request.Nickname = GetStringOrEmpty(InObject, TEXT("nickname"));
		Request.RequestedAt = GetTime(InObject, TEXT("requestedAt"));
		Request.ExpiresAt = GetTime(InObject, TEXT("expiresAt"));
		return Request;
	}

	FWarriorGuildRequirement ReadRequirement(const TSharedPtr<FJsonObject>& InObject)
	{
		FWarriorGuildRequirement Requirement;
		Requirement.Type = GetStringOrEmpty(InObject, TEXT("type"));
		const FString StageId = GetStringOrEmpty(InObject, TEXT("stageId"));
		Requirement.StageId = StageId.IsEmpty() ? NAME_None : FName(*StageId);
		Requirement.bMet = GetBoolOrFalse(InObject, TEXT("met"));
		return Requirement;
	}

	//쓰기 본문 { "requestId": …, (InExtraField): … }. 칸 이름은 명세 그대로
	FString BuildActionBody(const FString& InRequestId, const FString& InExtraField, const FString& InExtraValue)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("requestId"), InRequestId);
		if (!InExtraField.IsEmpty())
		{
			Object->SetStringField(InExtraField, InExtraValue);
		}
		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		FJsonSerializer::Serialize(Object, Writer);
		return Body;
	}

	const TCHAR* ActionName(EWarriorGuildAction InAction)
	{
		switch (InAction)
		{
		case EWarriorGuildAction::Create:	return TEXT("Create");
		case EWarriorGuildAction::Apply:	return TEXT("Apply");
		case EWarriorGuildAction::Cancel:	return TEXT("Cancel");
		case EWarriorGuildAction::Approve:	return TEXT("Approve");
		case EWarriorGuildAction::Reject:	return TEXT("Reject");
		case EWarriorGuildAction::Leave:	return TEXT("Leave");
		default:							return TEXT("Disband");
		}
	}
}

UWarriorGuildSubsystem* UWarriorGuildSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorGuildSubsystem>() : nullptr;
}

void UWarriorGuildSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.AddUniqueDynamic(this, &ThisClass::HandleLoginCompleted);
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
	Collection.InitializeDependency<UWarriorApiSubsystem>();
	if (UWarriorRealtimeSubsystem* Realtime = Collection.InitializeDependency<UWarriorRealtimeSubsystem>())
	{
		Realtime->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRealtimeStateChanged);
		RealtimeMessageHandle = Realtime->OnMessage.AddUObject(this, &ThisClass::HandleRealtimeMessage);
	}
}

void UWarriorGuildSubsystem::Deinitialize()
{
	if (RealtimeRefreshTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RealtimeRefreshTicker);
		RealtimeRefreshTicker.Reset();
	}

	if (UWarriorRealtimeSubsystem* Realtime = GetGameInstance()->GetSubsystem<UWarriorRealtimeSubsystem>())
	{
		Realtime->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleRealtimeStateChanged);
		Realtime->OnMessage.Remove(RealtimeMessageHandle);
	}
	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &ThisClass::HandleLoginCompleted);
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}

	Super::Deinitialize();
}

//~ Begin 조회

void UWarriorGuildSubsystem::RefreshMyGuild()
{
	//받는 중이면 끝난 뒤 한 번 더(그사이 바뀐 것을 놓치지 않게)
	if (bMyGuildInFlight)
	{
		bMyGuildDirty = true;
		return;
	}

	const int32 Serial = ++MyGuildSerial;
	const int32 SessionSerial = GetSessionSerial();
	bMyGuildInFlight = true;
	bMyGuildDirty = false;
	const bool bSent = Send(TEXT("GET"), WarriorGuildApi::MyGuildPath, FString(), FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial, SessionSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleMyGuildResponse(Serial, SessionSerial, Result);
			}
		});

	if (!bSent)
	{
		bMyGuildInFlight = false;
	}
}

void UWarriorGuildSubsystem::RequestGuildList(const FString& InNamePrefix)
{
	const int32 Serial = ++GuildListSerial;
	const int32 SessionSerial = GetSessionSerial();
	GuildListNamePrefix = InNamePrefix;
	bGuildListInFlight = true;

	FString Path = FString::Printf(TEXT("%s?size=%d"), WarriorGuildApi::GuildsPath, FMath::Clamp(GuildListPageSize, 1, 50));
	if (!InNamePrefix.IsEmpty())
	{
		Path += TEXT("&name=") + FGenericPlatformHttp::UrlEncode(InNamePrefix);
	}

	const bool bSent = Send(TEXT("GET"), Path, FString(), FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial, SessionSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleGuildListResponse(Serial, SessionSerial, false, Result);
			}
		});

	if (!bSent)
	{
		bGuildListInFlight = false;
	}
}

void UWarriorGuildSubsystem::RequestMoreGuilds()
{
	if (bGuildListInFlight || GuildListCursor.IsEmpty())
	{
		return;
	}

	const int32 Serial = GuildListSerial;
	const int32 SessionSerial = GetSessionSerial();
	bGuildListInFlight = true;

	//커서는 서버가 준 값 그대로(모양에 기대지 않음, 명세 G1)
	FString Path = FString::Printf(TEXT("%s?size=%d&cursor=%s"), WarriorGuildApi::GuildsPath,
		FMath::Clamp(GuildListPageSize, 1, 50), *FGenericPlatformHttp::UrlEncode(GuildListCursor));
	if (!GuildListNamePrefix.IsEmpty())
	{
		Path += TEXT("&name=") + FGenericPlatformHttp::UrlEncode(GuildListNamePrefix);
	}

	const bool bSent = Send(TEXT("GET"), Path, FString(), FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial, SessionSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleGuildListResponse(Serial, SessionSerial, true, Result);
			}
		});

	if (!bSent)
	{
		bGuildListInFlight = false;
	}
}

void UWarriorGuildSubsystem::RequestReceivedJoinRequests()
{
	const int32 Serial = ++JoinRequestsSerial;
	const int32 SessionSerial = GetSessionSerial();
	bJoinRequestsRequested = true;

	Send(TEXT("GET"), WarriorGuildApi::ReceivedJoinRequestsPath, FString(), FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial, SessionSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleJoinRequestsResponse(Serial, SessionSerial, Result);
			}
		});
}

void UWarriorGuildSubsystem::HandleMyGuildResponse(int32 InSerial, int32 InSessionSerial, const FWarriorApiResult& InResult)
{
	//로그아웃·다시 로그인으로 번호가 바뀌었으면 지난 응답이다
	if (InSerial != MyGuildSerial || !IsSameSession(InSessionSerial))
	{
		return;
	}
	bMyGuildInFlight = false;

	if (!InResult.bOk)
	{
		NotifyRequestFailed(InResult);
	}
	else
	{
		FWarriorMyGuild Parsed;
		if (ParseMyGuild(InResult.Body, Parsed))
		{
			ApplyMyGuild(Parsed);
		}
		else
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[Guild] My guild response could not be read."));
			OnRequestFailed.Broadcast(TEXT("INVALID_RESPONSE"), FWarriorErrorText::GetText(TEXT("INVALID_RESPONSE"), WarriorErrorContext::Guild));
		}
	}

	if (bMyGuildDirty)
	{
		RefreshMyGuild();
	}
}

void UWarriorGuildSubsystem::HandleGuildListResponse(int32 InSerial, int32 InSessionSerial, bool bInAppend, const FWarriorApiResult& InResult)
{
	if (InSerial != GuildListSerial || !IsSameSession(InSessionSerial))
	{
		return;
	}
	bGuildListInFlight = false;

	if (!InResult.bOk)
	{
		NotifyRequestFailed(InResult);
		return;
	}

	TArray<FWarriorGuildSummary> Guilds;
	FString NextCursor;
	if (!ParseGuildList(InResult.Body, Guilds, NextCursor))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Guild] Guild list response could not be read."));
		OnRequestFailed.Broadcast(TEXT("INVALID_RESPONSE"), FWarriorErrorText::GetText(TEXT("INVALID_RESPONSE"), WarriorErrorContext::Guild));
		return;
	}

	if (bInAppend)
	{
		GuildList.Append(Guilds);
	}
	else
	{
		GuildList = MoveTemp(Guilds);
	}
	GuildListCursor = NextCursor;
	OnGuildListUpdated.Broadcast();
}

void UWarriorGuildSubsystem::HandleJoinRequestsResponse(int32 InSerial, int32 InSessionSerial, const FWarriorApiResult& InResult)
{
	if (InSerial != JoinRequestsSerial || !IsSameSession(InSessionSerial))
	{
		return;
	}

	if (!InResult.bOk)
	{
		NotifyRequestFailed(InResult);
		return;
	}

	TArray<FWarriorGuildJoinRequest> JoinRequests;
	if (!ParseJoinRequests(InResult.Body, JoinRequests))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Guild] Join request list response could not be read."));
		OnRequestFailed.Broadcast(TEXT("INVALID_RESPONSE"), FWarriorErrorText::GetText(TEXT("INVALID_RESPONSE"), WarriorErrorContext::Guild));
		return;
	}

	ReceivedJoinRequests = MoveTemp(JoinRequests);
	OnJoinRequestsUpdated.Broadcast();
}

//~ End 조회

//~ Begin 쓰기

bool UWarriorGuildSubsystem::CreateGuild(const FString& InName)
{
	return SendAction(EWarriorGuildAction::Create, TEXT("POST"), WarriorGuildApi::GuildsPath, TEXT("name"), InName);
}

bool UWarriorGuildSubsystem::ApplyToGuild(const FString& InGuildId)
{
	return SendAction(EWarriorGuildAction::Apply, TEXT("POST"), WarriorGuildApi::MyJoinRequestsPath, TEXT("guildId"), InGuildId);
}

bool UWarriorGuildSubsystem::CancelJoinRequest(const FString& InJoinRequestId)
{
	//DELETE는 본문·requestId 없음(다시 보내도 결과가 같음, 명세 G6)
	return SendAction(EWarriorGuildAction::Cancel, TEXT("DELETE"),
		FString::Printf(TEXT("%s/%s"), WarriorGuildApi::MyJoinRequestsPath, *FGenericPlatformHttp::UrlEncode(InJoinRequestId)));
}

bool UWarriorGuildSubsystem::ApproveJoinRequest(const FString& InJoinRequestId)
{
	return SendAction(EWarriorGuildAction::Approve, TEXT("POST"),
		FString::Printf(TEXT("%s/%s/approve"), WarriorGuildApi::ReceivedJoinRequestsPath, *FGenericPlatformHttp::UrlEncode(InJoinRequestId)));
}

bool UWarriorGuildSubsystem::RejectJoinRequest(const FString& InJoinRequestId)
{
	return SendAction(EWarriorGuildAction::Reject, TEXT("POST"),
		FString::Printf(TEXT("%s/%s/reject"), WarriorGuildApi::ReceivedJoinRequestsPath, *FGenericPlatformHttp::UrlEncode(InJoinRequestId)));
}

bool UWarriorGuildSubsystem::LeaveGuild()
{
	return SendAction(EWarriorGuildAction::Leave, TEXT("POST"), WarriorGuildApi::LeavePath);
}

bool UWarriorGuildSubsystem::DisbandGuild()
{
	return SendAction(EWarriorGuildAction::Disband, TEXT("POST"), WarriorGuildApi::DisbandPath);
}

bool UWarriorGuildSubsystem::SendAction(EWarriorGuildAction InAction, const FString& InVerb, const FString& InPath, const FString& InExtraField, const FString& InExtraValue)
{
	if (bActionInFlight)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Guild] %s ignored. Previous action is still in flight."), WarriorGuildApi::ActionName(InAction));
		return false;
	}

	//새로 누른 동작에는 새 requestId(명세 "한 번만 반영"). 자동 재시도는 공통 처리가 같은 본문으로 한다
	const bool bHasBody = InVerb != TEXT("DELETE");
	const FString RequestId = bHasBody ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower) : FString();
	const FString Body = bHasBody ? WarriorGuildApi::BuildActionBody(RequestId, InExtraField, InExtraValue) : FString();

	const int32 SessionSerial = GetSessionSerial();
	bActionInFlight = true;
	const bool bSent = Send(InVerb, InPath, Body, RequestId,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InAction, SessionSerial](const FWarriorApiResult& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleActionResponse(InAction, SessionSerial, Result);
			}
		});

	if (!bSent)
	{
		bActionInFlight = false;
		return false;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Guild] %s sent. %s %s requestId=%s"), WarriorGuildApi::ActionName(InAction), *InVerb, *InPath, *RequestId);
	return true;
}

void UWarriorGuildSubsystem::HandleActionResponse(EWarriorGuildAction InAction, int32 InSessionSerial, const FWarriorApiResult& InResult)
{
	bActionInFlight = false;
	if (!IsSameSession(InSessionSerial))
	{
		return;
	}

	//401: 공통 처리가 이 콜백 뒤에 로그인 상태를 끝내고, 컨트롤러가 안내를 한 번 띄운다. 여기서 실패를 알리면 길드 화면이 같은 안내를 또 띄운다
	if (InResult.HttpStatus == 401)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Guild] %s ended by 401. The session is ending."), WarriorGuildApi::ActionName(InAction));
		return;
	}

	const bool bTouchesJoinRequests = InAction == EWarriorGuildAction::Approve || InAction == EWarriorGuildAction::Reject;

	if (InResult.bOk)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Guild] %s succeeded. status=%d"), WarriorGuildApi::ActionName(InAction), InResult.HttpStatus);

		//만들기 응답은 G4 모양이라 바로 적용한다. 나머지는 G4를 다시 받아 화면을 맞춘다
		FWarriorMyGuild Created;
		if (InAction == EWarriorGuildAction::Create && ParseMyGuild(InResult.Body, Created))
		{
			ApplyMyGuild(Created);
		}
		else
		{
			RefreshMyGuild();
		}
		if (bTouchesJoinRequests && bJoinRequestsRequested)
		{
			RequestReceivedJoinRequests();
		}
		FinishAction(InAction, true, FString(), FText::GetEmpty());
		return;
	}

	//403·404·409는 그사이 상태가 바뀌었다는 뜻(이미 길드가 생김, 해체됨, 신청이 처리됨 …). 화면을 서버 상태로 다시 맞춘다
	const int32 Status = InResult.HttpStatus;
	if (Status == 403 || Status == 404 || Status == 409)
	{
		RefreshMyGuild();
		if (bTouchesJoinRequests && bJoinRequestsRequested)
		{
			RequestReceivedJoinRequests();
		}
	}
	FinishAction(InAction, false, InResult.Error.Code, InResult.Rule.Text);
}

void UWarriorGuildSubsystem::FinishAction(EWarriorGuildAction InAction, bool bInSuccess, const FString& InCode, const FText& InMessage)
{
	OnGuildActionCompleted.Broadcast(InAction, bInSuccess, InCode, InMessage);
}

//~ End 쓰기

void UWarriorGuildSubsystem::ApplyMyGuild(const FWarriorMyGuild& InMyGuild)
{
	const FString OldGuildId = GetMyGuildId();
	MyGuild = InMyGuild;
	bMyGuildLoaded = true;
	const FString NewGuildId = GetMyGuildId();

	UE_LOG(LogProjectWarrior, Log, TEXT("[Guild] My guild updated. guildId=%s role=%s members=%d pending=%d myRequests=%d"),
		*NewGuildId, *UEnum::GetValueAsString(MyGuild.MyRole), MyGuild.Members.Num(), MyGuild.PendingJoinRequestCount, MyGuild.MyJoinRequests.Num());

	if (OldGuildId != NewGuildId)
	{
		OnMyGuildChanged.Broadcast(OldGuildId, NewGuildId);
	}
	OnMyGuildUpdated.Broadcast();
}

bool UWarriorGuildSubsystem::Send(const FString& InVerb, const FString& InPath, const FString& InBody, const FString& InRequestId,
	TFunction<void(const FWarriorApiResult&)>&& OnDone)
{
	UWarriorApiSubsystem* Api = UWarriorApiSubsystem::Get(this);
	if (!Api)
	{
		return false;
	}

	//길드 요청은 보관하지 않고 "다시 시도 / 종료"를 묻지 않는다(명세 "한 번만 반영" 게임 규칙)
	FWarriorApiRequest Request;
	Request.Verb = InVerb;
	Request.Path = InPath;
	Request.Body = InBody;
	Request.RequestId = InRequestId;
	Request.StoreKind = EWarriorPendingKind::None;
	Request.bAskOnFail = false;
	Request.Context = WarriorErrorContext::Guild;
	return Api->Send(Request, MoveTemp(OnDone));
}

bool UWarriorGuildSubsystem::IsSameSession(int32 InSessionSerial) const
{
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	return Auth && Auth->IsLoggedIn() && Auth->GetSessionSerial() == InSessionSerial;
}

int32 UWarriorGuildSubsystem::GetSessionSerial() const
{
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	return Auth ? Auth->GetSessionSerial() : 0;
}

void UWarriorGuildSubsystem::NotifyRequestFailed(const FWarriorApiResult& InResult)
{
	if (InResult.HttpStatus == 401)
	{
		return;
	}
	OnRequestFailed.Broadcast(InResult.Error.Code, InResult.Rule.Text);
}

//~ Begin 실시간

void UWarriorGuildSubsystem::HandleRealtimeMessage(const FString& InType, const FString& InId, const TSharedPtr<FJsonObject>& InData)
{
	if (InType == TEXT("PRESENCE_CHANGED"))
	{
		HandlePresenceChanged(InData);
		return;
	}

	//길드 알림은 "다시 받으라"는 신호로만 쓴다(유실·순서 바뀜이 있을 수 있음). 화면 값은 G4가 기준
	const bool bJoinRequestChanged = InType == TEXT("GUILD_JOIN_REQUESTED") || InType == TEXT("GUILD_MEMBER_JOINED");
	if (bJoinRequestChanged
		|| InType == TEXT("GUILD_MEMBER_LEFT")
		|| InType == TEXT("GUILD_DISBANDED")
		|| InType == TEXT("GUILD_JOIN_REJECTED"))
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Guild] Realtime %s id=%s. Refreshing soon."), *InType, *InId);
		ScheduleRealtimeRefresh(bJoinRequestChanged);
	}
}

void UWarriorGuildSubsystem::HandlePresenceChanged(const TSharedPtr<FJsonObject>& InData)
{
	FString GuildId;
	FString AccountId;
	bool bOnline = false;
	if (!ParsePresenceChanged(InData, GuildId, AccountId, bOnline))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Guild] PRESENCE_CHANGED could not be read."));
		return;
	}

	//G4를 받기 전이거나 다른 길드의 늦은 알림이면 버린다. 길드가 바뀐 것은 GUILD_* 알림과 G4가 따로 맞춘다
	if (!bMyGuildLoaded || GuildId != GetMyGuildId())
	{
		return;
	}

	bool bChanged = false;
	if (!SetMemberOnline(MyGuild, AccountId, bOnline, bChanged))
	{
		//목록에 없는 사람(막 들어온 길드원 등): 목록이 옛것이므로 G4로 맞춘다
		ScheduleRealtimeRefresh(false);
		return;
	}
	if (bChanged)
	{
		UE_LOG(LogProjectWarrior, Verbose, TEXT("[Guild] Member %s is now %s."), *AccountId, bOnline ? TEXT("online") : TEXT("offline"));
		OnMyGuildUpdated.Broadcast();
	}
}

void UWarriorGuildSubsystem::ScheduleRealtimeRefresh(bool bInJoinRequests)
{
	bRealtimeRefreshJoinRequests |= bInJoinRequests;
	if (RealtimeRefreshTicker.IsValid())
	{
		return;
	}
	RealtimeRefreshTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleRealtimeRefreshTick), FMath::Max(0.f, RealtimeRefreshDelaySeconds));
}

bool UWarriorGuildSubsystem::HandleRealtimeRefreshTick(float InDeltaTime)
{
	RealtimeRefreshTicker.Reset();
	const bool bJoinRequests = bRealtimeRefreshJoinRequests;
	bRealtimeRefreshJoinRequests = false;

	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || !Auth->IsLoggedIn())
	{
		return false;
	}

	RefreshMyGuild();
	//받은 신청 목록은 길드장이 연 적이 있을 때만(아니면 403)
	if (bJoinRequests && bJoinRequestsRequested && IsLeader())
	{
		RequestReceivedJoinRequests();
	}
	return false;
}

void UWarriorGuildSubsystem::HandleRealtimeStateChanged(EWarriorRealtimeState InState)
{
	if (InState != EWarriorRealtimeState::Connected)
	{
		return;
	}

	//처음 연결은 로그인 직후 G4와 겹친다. 다시 연결됐을 때만 끊긴 동안 놓친 알림을 G4로 보충한다
	if (bRealtimeWasConnected)
	{
		ScheduleRealtimeRefresh(true);
	}
	bRealtimeWasConnected = true;
}

//~ End 실시간

void UWarriorGuildSubsystem::HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message)
{
	//채팅이 팝업과 상관없이 GetMyGuildId를 쓰므로 로그인하자마자 받는다
	if (bSuccess)
	{
		RefreshMyGuild();
	}
}

void UWarriorGuildSubsystem::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	ResetState();
}

void UWarriorGuildSubsystem::ResetState()
{
	const FString OldGuildId = GetMyGuildId();

	if (RealtimeRefreshTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RealtimeRefreshTicker);
		RealtimeRefreshTicker.Reset();
	}

	//번호를 올려 응답을 기다리던 요청을 모두 버린다
	++MyGuildSerial;
	++GuildListSerial;
	++JoinRequestsSerial;

	MyGuild = FWarriorMyGuild();
	GuildList.Reset();
	ReceivedJoinRequests.Reset();
	GuildListNamePrefix.Reset();
	GuildListCursor.Reset();
	bMyGuildLoaded = false;
	bMyGuildInFlight = false;
	bMyGuildDirty = false;
	bGuildListInFlight = false;
	bJoinRequestsRequested = false;
	bActionInFlight = false;
	bRealtimeRefreshJoinRequests = false;
	bRealtimeWasConnected = false;

	if (!OldGuildId.IsEmpty())
	{
		OnMyGuildChanged.Broadcast(OldGuildId, FString());
	}
	OnMyGuildUpdated.Broadcast();
}

//~ Begin 응답 읽기

bool UWarriorGuildSubsystem::ParseMyGuild(const FString& InBody, FWarriorMyGuild& OutMyGuild)
{
	using namespace WarriorGuildApi;

	const TSharedPtr<FJsonObject> Data = ReadData(InBody);
	if (!Data.IsValid())
	{
		return false;
	}

	FWarriorMyGuild Result;
	//미소속이면 guild가 null
	if (const TSharedPtr<FJsonObject> Guild = GetObjectOrNull(Data, TEXT("guild")))
	{
		Result.bHasGuild = true;
		Result.Guild = ReadSummary(Guild);
		Result.MyRole = ReadRole(GetStringOrEmpty(Guild, TEXT("myRole")));
		if (Result.Guild.GuildId.IsEmpty())
		{
			return false;
		}
	}
	Result.Members = ReadArray<FWarriorGuildMember>(Data, TEXT("members"), ReadMember);
	Result.PendingJoinRequestCount = GetIntOrZero(Data, TEXT("pendingJoinRequestCount"));
	Result.MyJoinRequests = ReadArray<FWarriorGuildJoinRequest>(Data, TEXT("myJoinRequests"), ReadJoinRequest);
	if (const TSharedPtr<FJsonObject> Create = GetObjectOrNull(Data, TEXT("create")))
	{
		Result.bCanCreate = GetBoolOrFalse(Create, TEXT("allowed"));
		Result.CreateRequirements = ReadArray<FWarriorGuildRequirement>(Create, TEXT("requirements"), ReadRequirement);
	}

	OutMyGuild = MoveTemp(Result);
	return true;
}

bool UWarriorGuildSubsystem::ParseGuildList(const FString& InBody, TArray<FWarriorGuildSummary>& OutGuilds, FString& OutNextCursor)
{
	using namespace WarriorGuildApi;

	const TSharedPtr<FJsonObject> Data = ReadData(InBody);
	if (!Data.IsValid() || !Data->HasField(TEXT("guilds")))
	{
		return false;
	}

	OutGuilds = ReadArray<FWarriorGuildSummary>(Data, TEXT("guilds"), ReadSummary);
	//마지막 묶음이면 nextCursor가 null → 빈 값(HasMoreGuilds false)
	OutNextCursor = GetBoolOrFalse(Data, TEXT("hasNext")) ? GetStringOrEmpty(Data, TEXT("nextCursor")) : FString();
	return true;
}

bool UWarriorGuildSubsystem::ParseJoinRequests(const FString& InBody, TArray<FWarriorGuildJoinRequest>& OutJoinRequests)
{
	using namespace WarriorGuildApi;

	const TSharedPtr<FJsonObject> Data = ReadData(InBody);
	if (!Data.IsValid() || !Data->HasField(TEXT("joinRequests")))
	{
		return false;
	}

	OutJoinRequests = ReadArray<FWarriorGuildJoinRequest>(Data, TEXT("joinRequests"), ReadJoinRequest);
	return true;
}

bool UWarriorGuildSubsystem::ParsePresenceChanged(const TSharedPtr<FJsonObject>& InData, FString& OutGuildId, FString& OutAccountId, bool& bOutOnline)
{
	//값으로 바로 고치므로 칸이 빠진 메시지는 쓰지 않는다(GetBoolOrFalse처럼 없으면 false로 보면 잘못 끊김으로 보임).
	//online은 JSON true·false만 받는다(TryGetBoolField는 문자열도 bool로 바꿔 읽음)
	if (!InData.IsValid()
		|| !InData->TryGetStringField(TEXT("guildId"), OutGuildId)
		|| !InData->TryGetStringField(TEXT("accountId"), OutAccountId)
		|| !InData->HasTypedField<EJson::Boolean>(TEXT("online")))
	{
		return false;
	}
	bOutOnline = InData->GetBoolField(TEXT("online"));
	return !OutGuildId.IsEmpty() && !OutAccountId.IsEmpty();
}

bool UWarriorGuildSubsystem::SetMemberOnline(FWarriorMyGuild& InOutMyGuild, const FString& InAccountId, bool bInOnline, bool& bOutChanged)
{
	bOutChanged = false;
	FWarriorGuildMember* Member = InOutMyGuild.Members.FindByPredicate([&InAccountId](const FWarriorGuildMember& InMember)
	{
		return InMember.AccountId == InAccountId;
	});
	if (!Member)
	{
		return false;
	}
	bOutChanged = Member->bOnline != bInOnline;
	Member->bOnline = bInOnline;
	return true;
}

//~ End 응답 읽기
