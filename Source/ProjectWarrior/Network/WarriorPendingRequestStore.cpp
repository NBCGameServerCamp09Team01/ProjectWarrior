// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPendingRequestStore.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ProjectWarrior/ProjectWarrior.h"

namespace WarriorPendingStore
{
	const TCHAR* const RootFolder = TEXT("PendingRequests");
	const TCHAR* const Extension = TEXT(".json");
	const TCHAR* const CorruptedExtension = TEXT(".bad");

	FString Sha1Hex(const FString& InText)
	{
		const FTCHARToUTF8 Utf8(*InText);
		FSHAHash Hash;
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
		return Hash.ToString();
	}

	//파일 이름에 쓸 수 있는 requestId인가(소문자 하이픈 GUID 모양). 경로를 벗어나는 이름을 막는다
	bool IsSafeFileName(const FString& InName)
	{
		if (InName.IsEmpty() || InName.Len() > 64)
		{
			return false;
		}
		for (const TCHAR Char : InName)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	FDateTime ToWholeSeconds(const FDateTime& InTime)
	{
		return FDateTime(InTime.GetTicks() - InTime.GetTicks() % ETimespan::TicksPerSecond);
	}

	FString FilePath(const FString& InAccountId, const FString& InRequestId)
	{
		return FPaths::Combine(FWarriorPendingRequestStore::GetAccountDir(InAccountId), InRequestId + Extension);
	}
}

FString FWarriorPendingRequestStore::GetAccountDir(const FString& InAccountId)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), WarriorPendingStore::RootFolder, WarriorPendingStore::Sha1Hex(InAccountId).Left(16));
}

FString FWarriorPendingRequestStore::ComputeHash(const FWarriorPendingRequest& InEntry)
{
	//칸을 구분자로 이어 해시한다. 칸 순서를 바꾸면 SchemaVersion을 올린다
	const FString Joined = FString::Printf(TEXT("%d|%s|%s|%s|%s|%s|%s|%s|%s"),
		InEntry.SchemaVersion,
		*UEnum::GetValueAsString(InEntry.Kind),
		*InEntry.Verb, *InEntry.Path, *InEntry.Body, *InEntry.RequestId, *InEntry.StoreKey,
		*InEntry.CreatedAt.ToIso8601(), *InEntry.ExpiresAt.ToIso8601());
	return WarriorPendingStore::Sha1Hex(Joined);
}

bool FWarriorPendingRequestStore::Save(const FString& InAccountId, FWarriorPendingRequest InEntry)
{
	if (InAccountId.IsEmpty() || !WarriorPendingStore::IsSafeFileName(InEntry.RequestId))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Pending] Not saved. Missing account or bad requestId '%s'."), *InEntry.RequestId);
		return false;
	}

	//JSON에서 다시 읽으면 시각의 밀리초가 빠지므로(FJsonObjectConverter) 초 단위로 맞춘 뒤 해시한다
	InEntry.CreatedAt = WarriorPendingStore::ToWholeSeconds(InEntry.CreatedAt);
	InEntry.ExpiresAt = WarriorPendingStore::ToWholeSeconds(InEntry.ExpiresAt);
	InEntry.Hash = ComputeHash(InEntry);

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(InEntry, Json))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Pending] Not saved. JSON could not be built. requestId=%s"), *InEntry.RequestId);
		return false;
	}

	const FString Path = WarriorPendingStore::FilePath(InAccountId, InEntry.RequestId);
	if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Pending] Not saved. File write failed: %s"), *Path);
		return false;
	}

	//본문은 로그에 남기지 않는다
	UE_LOG(LogProjectWarrior, Log, TEXT("[Pending] Saved %s requestId=%s key=%s expiresAt=%s"),
		*UEnum::GetValueAsString(InEntry.Kind), *InEntry.RequestId, *InEntry.StoreKey, *InEntry.ExpiresAt.ToIso8601());
	return true;
}

TArray<FWarriorPendingRequest> FWarriorPendingRequestStore::LoadAll(const FString& InAccountId, int32& OutCorruptedCount)
{
	OutCorruptedCount = 0;
	TArray<FWarriorPendingRequest> Entries;
	if (InAccountId.IsEmpty())
	{
		return Entries;
	}

	const FString Dir = GetAccountDir(InAccountId);
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *FPaths::Combine(Dir, FString(TEXT("*")) + WarriorPendingStore::Extension), true, false);

	for (const FString& FileName : FileNames)
	{
		const FString Path = FPaths::Combine(Dir, FileName);
		FString Json;
		FWarriorPendingRequest Entry;
		const TCHAR* Problem = nullptr;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			Problem = TEXT("file could not be read");
		}
		else if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Entry))
		{
			Problem = TEXT("JSON could not be read");
		}
		else if (Entry.Kind == EWarriorPendingKind::None)
		{
			Problem = TEXT("unknown kind");
		}
		else if (Entry.Hash.IsEmpty() || Entry.Hash != ComputeHash(Entry))
		{
			Problem = TEXT("hash mismatch");
		}
		else if (FPaths::GetBaseFilename(FileName) != Entry.RequestId)
		{
			Problem = TEXT("file name does not match requestId");
		}

		if (Problem)
		{
			//보내지 않는다. 확인할 수 있게 지우지 않고 이름만 바꾼다
			++OutCorruptedCount;
			const FString BadPath = Path + WarriorPendingStore::CorruptedExtension;
			IFileManager::Get().Move(*BadPath, *Path, true);
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Pending] Corrupted file moved aside (%s): %s"), Problem, *BadPath);
			continue;
		}

		Entries.Add(MoveTemp(Entry));
	}

	Entries.Sort([](const FWarriorPendingRequest& A, const FWarriorPendingRequest& B) { return A.CreatedAt < B.CreatedAt; });
	return Entries;
}

bool FWarriorPendingRequestStore::Remove(const FString& InAccountId, const FString& InRequestId)
{
	if (InAccountId.IsEmpty() || !WarriorPendingStore::IsSafeFileName(InRequestId))
	{
		return false;
	}

	const FString Path = WarriorPendingStore::FilePath(InAccountId, InRequestId);
	const bool bDeleted = IFileManager::Get().Delete(*Path, false, false, true);
	UE_LOG(LogProjectWarrior, Log, TEXT("[Pending] Removed requestId=%s deleted=%s"), *InRequestId, bDeleted ? TEXT("true") : TEXT("false"));
	return bDeleted;
}

bool FWarriorPendingRequestStore::FindByKey(const FString& InAccountId, EWarriorPendingKind InKind, const FString& InStoreKey, FWarriorPendingRequest& OutEntry)
{
	int32 Corrupted = 0;
	for (FWarriorPendingRequest& Entry : LoadAll(InAccountId, Corrupted))
	{
		if (Entry.Kind == InKind && Entry.StoreKey.Equals(InStoreKey, ESearchCase::IgnoreCase))
		{
			OutEntry = MoveTemp(Entry);
			return true;
		}
	}
	return false;
}
