// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorRankingSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"

//랭킹 응답 읽기. 다른 기능의 도우미를 쓰지 않고 이 파일 안에 둔다(랭킹 단독, 명세 ranking-api.md v1)
namespace WarriorRankingApi
{
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

	//null·없음이면 InDefault
	double GetNumberOr(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField, double InDefault)
	{
		double Value = 0.0;
		return InObject->TryGetNumberField(InField, Value) ? Value : InDefault;
	}

	FDateTime GetTime(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		FDateTime Time;
		return FDateTime::ParseIso8601(*GetStringOrEmpty(InObject, InField), Time) ? Time : FDateTime();
	}

	//줄 하나(items[]·top·내 줄 공통 칸). 보드에 맞지 않는 칸은 null → -1·None
	FWarriorRankingEntry ReadEntry(const TSharedPtr<FJsonObject>& InObject)
	{
		FWarriorRankingEntry Entry;
		Entry.Rank = static_cast<int32>(GetNumberOr(InObject, TEXT("rank"), 0.0));
		Entry.AccountId = GetStringOrEmpty(InObject, TEXT("accountId"));
		Entry.Nickname = GetStringOrEmpty(InObject, TEXT("nickname"));
		Entry.Score = static_cast<int32>(GetNumberOr(InObject, TEXT("score"), 0.0));
		Entry.AchievedAt = GetTime(InObject, TEXT("achievedAt"));
		Entry.PlayTimeSeconds = static_cast<float>(GetNumberOr(InObject, TEXT("playTimeSeconds"), -1.0));
		Entry.ClearedStageCount = static_cast<int32>(GetNumberOr(InObject, TEXT("clearedStageCount"), -1.0));
		const FString Furthest = GetStringOrEmpty(InObject, TEXT("furthestStageId"));
		Entry.FurthestStageId = Furthest.IsEmpty() ? NAME_None : FName(*Furthest);
		return Entry;
	}

	const TCHAR* KindSegment(EWarriorRankingBoardKind InKind)
	{
		switch (InKind)
		{
		case EWarriorRankingBoardKind::Stage:
			return TEXT("stages/");
		case EWarriorRankingBoardKind::Region:
			return TEXT("regions/");
		case EWarriorRankingBoardKind::Total:
			return TEXT("total");
		default:
			return TEXT("progress");
		}
	}

	const TCHAR* PeriodValue(EWarriorRankingPeriod InPeriod)
	{
		return InPeriod == EWarriorRankingPeriod::Weekly ? TEXT("weekly") : TEXT("all");
	}

	//"&difficulty=n". 진행 보드는 난이도 쿼리가 없다(명세 쿼리 표: RK1~4·RK7·RK8만)
	FString DifficultyQuery(const FWarriorRankingBoardKey& InKey)
	{
		return InKey.Kind == EWarriorRankingBoardKind::Progress ? FString() : FString::Printf(TEXT("&difficulty=%d"), InKey.Difficulty);
	}
}

UWarriorRankingSubsystem* UWarriorRankingSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorRankingSubsystem>() : nullptr;
}

void UWarriorRankingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
}

void UWarriorRankingSubsystem::Deinitialize()
{
	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}

	Super::Deinitialize();
}

void UWarriorRankingSubsystem::RequestBoard(EWarriorRankingBoardKind InKind, FName InTargetId, EWarriorRankingPeriod InPeriod, int32 InLimit, int32 InDifficulty)
{
	FWarriorRankingBoardKey Key;
	Key.Kind = InKind;
	//스테이지·지역 보드만 대상이 있다
	const bool bHasTarget = InKind == EWarriorRankingBoardKind::Stage || InKind == EWarriorRankingBoardKind::Region;
	Key.TargetId = bHasTarget ? InTargetId : NAME_None;
	//명세: limit 1~50
	Limit = FMath::Clamp(InLimit, 1, 50);
	//진행 보드는 전체 기간만(명세 RK5), 난이도도 없다
	const bool bProgress = InKind == EWarriorRankingBoardKind::Progress;
	Key.Period = bProgress ? EWarriorRankingPeriod::All : InPeriod;
	Key.Difficulty = bProgress ? 0 : FMath::Max(0, InDifficulty);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Ranking] Board requested. kind=%s target=%s period=%s difficulty=%d"),
		*UEnum::GetValueAsString(Key.Kind), *Key.TargetId.ToString(), WarriorRankingApi::PeriodValue(Key.Period), Key.Difficulty);

	const int32 Serial = ++BoardSerial;
	BoardKey = Key;
	PeriodKey.Reset();
	Entries.Reset();
	Mine = FWarriorRankingMine();
	NextCursor.Reset();
	bRequestingBoard = true;
	bListArrived = false;
	bMineArrived = false;

	const bool bListSent = Send(BuildListPath(Key, FString()),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleListResponse(Serial, false, Status, Body);
			}
		});
	const bool bMineSent = bListSent && Send(BuildMinePath(Key),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleMineResponse(Serial, Status, Body);
			}
		});

	if (!bListSent || !bMineSent)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Ranking] Board request not sent (not logged in)."));
		FailBoard(Serial, 401, FString());
	}
}

void UWarriorRankingSubsystem::RequestMore()
{
	if (bRequestingBoard || NextCursor.IsEmpty())
	{
		return;
	}

	const int32 Serial = BoardSerial;
	bRequestingBoard = true;
	const bool bSent = Send(BuildListPath(BoardKey, NextCursor),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), Serial](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleListResponse(Serial, true, Status, Body);
			}
		});

	if (!bSent)
	{
		FailBoard(Serial, 401, FString());
	}
}

EWarriorRankingFailure UWarriorRankingSubsystem::ClassifyRankingFailure(int32 InStatus, const FString& InBody, FString& OutCode)
{
	//S5 연동 지점: 공통 오류 분류가 생기면 아래를 그쪽 호출로 바꾼다
	if (InStatus == 0)
	{
		OutCode = TEXT("NETWORK_ERROR");
		return EWarriorRankingFailure::ServerUnavailable;
	}
	if (InStatus == -1)
	{
		OutCode = TEXT("INVALID_RESPONSE");
		return EWarriorRankingFailure::ServerUnavailable;
	}

	OutCode.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InBody);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		OutCode = WarriorRankingApi::GetStringOrEmpty(Root, TEXT("code"));
	}
	if (OutCode.IsEmpty())
	{
		OutCode = FString::Printf(TEXT("HTTP_%d"), InStatus);
	}

	if (InStatus == 401)
	{
		return EWarriorRankingFailure::SessionEnded;
	}
	if (OutCode == TEXT("RANK_INVALID_CURSOR"))
	{
		return EWarriorRankingFailure::InvalidCursor;
	}
	if (OutCode == TEXT("STAGE_NOT_FOUND") || OutCode == TEXT("REGION_NOT_FOUND"))
	{
		return EWarriorRankingFailure::NotFound;
	}
	if (InStatus >= 500)
	{
		return EWarriorRankingFailure::ServerUnavailable;
	}
	return EWarriorRankingFailure::Other;
}

bool UWarriorRankingSubsystem::Send(const FString& InPath, TFunction<void(int32, const FString&)>&& OnDone)
{
	//요청 길 연동 지점: S5가 요청 길을 바꾸면 여기만 바꾼다
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	return Auth && Auth->SendAuthorized(TEXT("GET"), InPath, FString(), MoveTemp(OnDone));
}

FString UWarriorRankingSubsystem::BuildListPath(const FWarriorRankingBoardKey& InKey, const FString& InCursor) const
{
	FString Path = FString(TEXT("/rankings/")) + WarriorRankingApi::KindSegment(InKey.Kind);
	if (InKey.Kind == EWarriorRankingBoardKind::Stage || InKey.Kind == EWarriorRankingBoardKind::Region)
	{
		Path += InKey.TargetId.ToString();
	}
	Path += FString::Printf(TEXT("?period=%s&limit=%d"), WarriorRankingApi::PeriodValue(InKey.Period), Limit);
	Path += WarriorRankingApi::DifficultyQuery(InKey);
	if (!InCursor.IsEmpty())
	{
		Path += TEXT("&cursor=") + FGenericPlatformHttp::UrlEncode(InCursor);
	}
	return Path;
}

FString UWarriorRankingSubsystem::BuildMinePath(const FWarriorRankingBoardKey& InKey) const
{
	FString Path = FString(TEXT("/accounts/me/rankings/")) + WarriorRankingApi::KindSegment(InKey.Kind);
	if (InKey.Kind == EWarriorRankingBoardKind::Stage || InKey.Kind == EWarriorRankingBoardKind::Region)
	{
		Path += InKey.TargetId.ToString();
	}
	return Path + FString::Printf(TEXT("?period=%s"), WarriorRankingApi::PeriodValue(InKey.Period)) + WarriorRankingApi::DifficultyQuery(InKey);
}

void UWarriorRankingSubsystem::HandleListResponse(int32 InSerial, bool bInAppend, int32 Status, const FString& Body)
{
	if (InSerial != BoardSerial)
	{
		return;
	}
	if (Status != 200)
	{
		FailBoard(InSerial, Status, Body);
		return;
	}

	const TSharedPtr<FJsonObject> Data = WarriorRankingApi::ReadData(Body);
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Data.IsValid() || !Data->TryGetArrayField(TEXT("items"), Items) || !Items)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Ranking] List response could not be read."));
		FailBoard(InSerial, -1, FString());
		return;
	}

	if (const TSharedPtr<FJsonObject> Board = WarriorRankingApi::GetObjectOrNull(Data, TEXT("board")))
	{
		PeriodKey = WarriorRankingApi::GetStringOrEmpty(Board, TEXT("periodKey"));
	}
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		const TSharedPtr<FJsonObject>* ItemObject = nullptr;
		if (Item.IsValid() && Item->TryGetObject(ItemObject) && ItemObject)
		{
			Entries.Add(WarriorRankingApi::ReadEntry(*ItemObject));
		}
	}
	NextCursor = WarriorRankingApi::GetStringOrEmpty(Data, TEXT("nextCursor"));

	if (bInAppend)
	{
		bRequestingBoard = false;
		MarkMine();
		OnBoardUpdated.Broadcast();
		return;
	}
	bListArrived = true;
	FinishBoardIfReady(InSerial);
}

void UWarriorRankingSubsystem::HandleMineResponse(int32 InSerial, int32 Status, const FString& Body)
{
	if (InSerial != BoardSerial)
	{
		return;
	}
	if (Status != 200)
	{
		FailBoard(InSerial, Status, Body);
		return;
	}

	const TSharedPtr<FJsonObject> Data = WarriorRankingApi::ReadData(Body);
	if (!Data.IsValid())
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Ranking] Mine response could not be read."));
		FailBoard(InSerial, -1, FString());
		return;
	}

	Mine = FWarriorRankingMine();
	Mine.TotalCount = static_cast<int32>(WarriorRankingApi::GetNumberOr(Data, TEXT("totalCount"), 0.0));
	Mine.bHasRecord = WarriorRankingApi::GetNumberOr(Data, TEXT("rank"), 0.0) > 0.0;
	if (Mine.bHasRecord)
	{
		Mine.Entry = WarriorRankingApi::ReadEntry(Data);
		Mine.Entry.bIsMine = true;
	}
	bMineArrived = true;
	FinishBoardIfReady(InSerial);
}

void UWarriorRankingSubsystem::FinishBoardIfReady(int32 InSerial)
{
	if (InSerial != BoardSerial || !bListArrived || !bMineArrived)
	{
		return;
	}
	bRequestingBoard = false;
	MarkMine();
	OnBoardUpdated.Broadcast();
}

void UWarriorRankingSubsystem::FailBoard(int32 InSerial, int32 Status, const FString& Body)
{
	if (InSerial != BoardSerial)
	{
		return;
	}

	FString Code;
	const EWarriorRankingFailure Failure = ClassifyRankingFailure(Status, Body, Code);
	if (Failure == EWarriorRankingFailure::InvalidCursor)
	{
		//서버가 준 커서가 더는 통하지 않음(주가 바뀜 등) → 알리지 않고 첫 묶음부터(명세 RANK_INVALID_CURSOR)
		UE_LOG(LogProjectWarrior, Log, TEXT("[Ranking] Cursor no longer valid. Reloading first page."));
		RequestBoard(BoardKey.Kind, BoardKey.TargetId, BoardKey.Period, Limit, BoardKey.Difficulty);
		return;
	}

	//목록·내 줄 중 하나라도 실패하면 이 요청은 끝(뒤에 오는 응답은 번호로 버림)
	++BoardSerial;
	bRequestingBoard = false;
	UE_LOG(LogProjectWarrior, Warning, TEXT("[Ranking] Board request failed. status=%d code=%s"), Status, *Code);
	OnBoardFailed.Broadcast(Failure, Code);
}

void UWarriorRankingSubsystem::MarkMine()
{
	for (FWarriorRankingEntry& Entry : Entries)
	{
		Entry.bIsMine = Mine.bHasRecord && Entry.Rank == Mine.Entry.Rank && Entry.AccountId == Mine.Entry.AccountId;
	}
}

void UWarriorRankingSubsystem::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	//순위·내 줄은 계정마다 다르므로 모두 비운다
	++BoardSerial;
	BoardKey = FWarriorRankingBoardKey();
	PeriodKey.Reset();
	Entries.Reset();
	Mine = FWarriorRankingMine();
	NextCursor.Reset();
	bRequestingBoard = false;
}
