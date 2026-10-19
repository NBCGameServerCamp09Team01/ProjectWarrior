// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSkillUnlockSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"

#define LOCTEXT_NAMESPACE "WarriorSkillUnlock"

namespace WarriorSkillUnlockApi
{
	const TCHAR* UnlockPath = TEXT("/accounts/me/skill-unlocks");
	const TCHAR* SkillsPath = TEXT("/accounts/me/skills");
	const TCHAR* RequirementsNotMet = TEXT("SKILL_REQUIREMENTS_NOT_MET");

	//실패 응답 본문을 읽는다. 연결 실패(Status 0)는 NETWORK_ERROR, 본문을 못 읽으면 HTTP_<상태>
	FWarriorApiError ParseError(int32 InStatus, const FString& InBody)
	{
		FWarriorApiError Error;
		if (InStatus == 0)
		{
			Error.Code = TEXT("NETWORK_ERROR");
			Error.Retryable = true;
		}
		else if (!FJsonObjectConverter::JsonObjectStringToUStruct(InBody, &Error) || Error.Code.IsEmpty())
		{
			Error.Code = FString::Printf(TEXT("HTTP_%d"), InStatus);
		}
		return Error;
	}

	TSharedPtr<FJsonObject> ReadObject(const FString& InBody)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InBody);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}
}

UWarriorSkillUnlockSubsystem* UWarriorSkillUnlockSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorSkillUnlockSubsystem>() : nullptr;
}

bool UWarriorSkillUnlockSubsystem::RequestUnlock(FGameplayTag SkillTag)
{
	if (bUnlockInFlight)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Request ignored. Previous one is still in flight."));
		return false;
	}
	if (!SkillTag.IsValid())
	{
		return false;
	}

	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || !Auth->IsLoggedIn())
	{
		//로컬 해금 길은 두지 않는다(S3 D8과 같은 규칙). 화면에 안내만 띄운다
		UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Not logged in. Request not sent."));
		FinishUnlock(false, SkillTag, TEXT("NOT_LOGGED_IN"), ErrorToText(TEXT("NOT_LOGGED_IN"), 0));
		return false;
	}

	FWarriorSkillUnlockRequestDto RequestDto;
	RequestDto.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	RequestDto.SkillTag = SkillTag.ToString();

	FString RequestBody;
	if (!FJsonObjectConverter::UStructToJsonObjectString(RequestDto, RequestBody, 0, 0, 0, nullptr, false))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[SkillUnlock] Request body could not be built."));
		return false;
	}

	LastRequirements.Reset();
	bUnlockInFlight = true;
	if (!SendUnlock(SkillTag, RequestBody, 1))
	{
		bUnlockInFlight = false;
		return false;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[SkillUnlock] Unlock sent. skill=%s requestId=%s"), *SkillTag.ToString(), *RequestDto.RequestId);
	return true;
}

bool UWarriorSkillUnlockSubsystem::RequestSkills()
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (bSkillsInFlight || !Auth || !Auth->IsLoggedIn())
	{
		return false;
	}

	bSkillsInFlight = true;
	const int32 SessionSerial = Auth->GetSessionSerial();
	const bool bStarted = Auth->SendAuthorized(TEXT("GET"), WarriorSkillUnlockApi::SkillsPath, FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), SessionSerial](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleSkillsResponse(SessionSerial, Status, Body);
			}
		});
	if (!bStarted)
	{
		bSkillsInFlight = false;
	}
	return bStarted;
}

bool UWarriorSkillUnlockSubsystem::HasServerSkills() const
{
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	return Auth && Auth->IsLoggedIn() && ServerSkillsSessionSerial == Auth->GetSessionSerial();
}

bool UWarriorSkillUnlockSubsystem::SendUnlock(FGameplayTag InSkillTag, const FString& InRequestBody, int32 InAttempt)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth)
	{
		return false;
	}

	const bool bStarted = Auth->SendAuthorized(TEXT("POST"), WarriorSkillUnlockApi::UnlockPath, InRequestBody,
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InSkillTag, InRequestBody, InAttempt](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleUnlockResponse(InSkillTag, InRequestBody, InAttempt, Status, Body);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Request could not be sent (not logged in or failed to start)."));
	}
	return bStarted;
}

void UWarriorSkillUnlockSubsystem::HandleUnlockResponse(FGameplayTag InSkillTag, const FString& InRequestBody, int32 InAttempt, int32 Status, const FString& Body)
{
	//── 200: SAVED 또는 ALREADY_SAVED. 응답 account를 로그인 응답과 같은 길로 적용한다(OnAccountChanged → 화면 갱신)
	if (Status == 200)
	{
		FWarriorSkillUnlockResponseDto ResponseDto;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Body, &ResponseDto))
		{
			UE_LOG(LogProjectWarrior, Error, TEXT("[SkillUnlock] Unlock saved but the response could not be read."));
			FinishUnlock(false, InSkillTag, TEXT("INVALID_RESPONSE"), ErrorToText(TEXT("INVALID_RESPONSE"), Status));
			return;
		}

		if (UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this))
		{
			Auth->ApplyAccountSnapshot(ResponseDto.Data.Account);
		}
		UE_LOG(LogProjectWarrior, Log, TEXT("[SkillUnlock] Unlock saveStatus %s. skill=%s statPoints=%d version=%lld serverRequestId=%s"),
			*ResponseDto.Data.SaveStatus, *InSkillTag.ToString(), ResponseDto.Data.Result.StatPoints, ResponseDto.Data.Result.Version,
			*ResponseDto.Meta.RequestId);
		FinishUnlock(true, InSkillTag, FString(), FText::GetEmpty());
		//해금으로 다른 스킬의 선행·포인트가 바뀌었으니 서버 진행도를 다시 받는다
		RequestSkills();
		return;
	}

	const FWarriorApiError Error = WarriorSkillUnlockApi::ParseError(Status, Body);

	//── retryable: 같은 본문(같은 requestId)으로 다시. 401은 Auth가 이미 로그인 상태를 끝냈고 retryable이 아니다
	if (Error.Retryable && InAttempt < FMath::Max(1, UnlockMaxAttempts))
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Unlock failed, sending again. status=%d code=%s attempt=%d"),
			Status, *Error.Code, InAttempt);
		if (SendUnlock(InSkillTag, InRequestBody, InAttempt + 1))
		{
			return;
		}
	}

	//── 조건 미충족: 본문의 requirements(조건 전부)를 남겨 화면이 그 스킬 진행도를 다시 그리게 한다
	if (Error.Code == WarriorSkillUnlockApi::RequirementsNotMet)
	{
		const TSharedPtr<FJsonObject> Root = WarriorSkillUnlockApi::ReadObject(Body);
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Root.IsValid() && Root->TryGetArrayField(TEXT("requirements"), Values))
		{
			ParseRequirements(*Values, LastRequirements);
		}
	}

	FString Unmet;
	for (const FWarriorServerSkillRequirement& Requirement : LastRequirements)
	{
		if (!Requirement.bMet)
		{
			Unmet += TEXT(" ") + Requirement.Type;
		}
	}
	UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Unlock not saved. skill=%s status=%d code=%s retryable=%s attempt=%d unmet=[%s ]"),
		*InSkillTag.ToString(), Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"), InAttempt, *Unmet);
	FinishUnlock(false, InSkillTag, Error.Code, ErrorToText(Error.Code, Status));
}

void UWarriorSkillUnlockSubsystem::HandleSkillsResponse(int32 InSessionSerial, int32 Status, const FString& Body)
{
	bSkillsInFlight = false;

	//보낸 뒤 로그아웃·다시 로그인했으면 지난 세션의 값이라 버린다
	const UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);
	if (!Auth || Auth->GetSessionSerial() != InSessionSerial)
	{
		return;
	}

	const TSharedPtr<FJsonObject> Root = Status == 200 ? WarriorSkillUnlockApi::ReadObject(Body) : nullptr;
	const TSharedPtr<FJsonObject>* Data = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
	if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("data"), Data) || !(*Data)->TryGetArrayField(TEXT("skills"), Skills))
	{
		const FWarriorApiError Error = WarriorSkillUnlockApi::ParseError(Status, Body);
		UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Skills not received. status=%d code=%s"), Status, *Error.Code);
		OnSkillsReceived.Broadcast(false);
		return;
	}

	ServerSkills.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *Skills)
	{
		const TSharedPtr<FJsonObject>* SkillObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(SkillObject))
		{
			continue;
		}
		FWarriorServerSkillView& View = ServerSkills.AddDefaulted_GetRef();
		View.SkillTag = FGameplayTag::RequestGameplayTag(FName((*SkillObject)->GetStringField(TEXT("skillTag"))), false);
		View.State = ParseState((*SkillObject)->GetStringField(TEXT("state")));
		View.Cost = static_cast<int32>((*SkillObject)->GetNumberField(TEXT("cost")));
		const TArray<TSharedPtr<FJsonValue>>* Requirements = nullptr;
		if ((*SkillObject)->TryGetArrayField(TEXT("requirements"), Requirements))
		{
			ParseRequirements(*Requirements, View.Requirements);
		}
		if (!View.SkillTag.IsValid())
		{
			//UE에 없는 태그(서버 마스터에만 있는 스킬). 화면이 쓰지 않으므로 버린다
			UE_LOG(LogProjectWarrior, Warning, TEXT("[SkillUnlock] Server skill %s is not a known gameplay tag."), *(*SkillObject)->GetStringField(TEXT("skillTag")));
			ServerSkills.Pop();
		}
	}
	ServerSkillsSessionSerial = InSessionSerial;
	UE_LOG(LogProjectWarrior, Log, TEXT("[SkillUnlock] Skills received. count=%d"), ServerSkills.Num());
	OnSkillsReceived.Broadcast(true);
}

void UWarriorSkillUnlockSubsystem::FinishUnlock(bool bInSuccess, FGameplayTag InSkillTag, const FString& InCode, const FText& InMessage)
{
	bUnlockInFlight = false;
	OnUnlockCompleted.Broadcast(bInSuccess, InSkillTag, InCode, InMessage);
}

void UWarriorSkillUnlockSubsystem::ParseRequirements(const TArray<TSharedPtr<FJsonValue>>& InValues, TArray<FWarriorServerSkillRequirement>& OutRequirements)
{
	OutRequirements.Reset();
	for (const TSharedPtr<FJsonValue>& Value : InValues)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object))
		{
			continue;
		}
		FWarriorServerSkillRequirement& Requirement = OutRequirements.AddDefaulted_GetRef();
		Requirement.Type = (*Object)->GetStringField(TEXT("type"));
		(*Object)->TryGetStringField(TEXT("key"), Requirement.Key);
		Requirement.Target = (*Object)->GetNumberField(TEXT("target"));
		Requirement.bHasCurrent = (*Object)->TryGetNumberField(TEXT("current"), Requirement.Current);
		(*Object)->TryGetBoolField(TEXT("met"), Requirement.bMet);
		(*Object)->TryGetBoolField(TEXT("upperBound"), Requirement.bUpperBound);
	}
}

EWarriorSkillState UWarriorSkillUnlockSubsystem::ParseState(const FString& InState)
{
	if (InState == TEXT("UNLOCKED"))
	{
		return EWarriorSkillState::Unlocked;
	}
	if (InState == TEXT("UNLOCKABLE"))
	{
		return EWarriorSkillState::Unlockable;
	}
	if (InState == TEXT("NEED_POINTS"))
	{
		return EWarriorSkillState::NeedPoints;
	}
	return EWarriorSkillState::Locked;
}

bool UWarriorSkillUnlockSubsystem::ToRequirementType(const FString& InServerType, EWarriorSkillRequirementType& OutType)
{
	static const TMap<FString, EWarriorSkillRequirementType> Types = {
		{ TEXT("ACCOUNT_LEVEL"), EWarriorSkillRequirementType::AccountLevel },
		{ TEXT("TOTAL_KILLS"), EWarriorSkillRequirementType::TotalKills },
		{ TEXT("STAGES_CLEARED"), EWarriorSkillRequirementType::StagesCleared },
		{ TEXT("PLAY_TIME_SECONDS"), EWarriorSkillRequirementType::PlayTimeSeconds },
		{ TEXT("MAX_WAVE_REACHED"), EWarriorSkillRequirementType::MaxWaveReached },
		{ TEXT("BEST_CLEAR_TIME"), EWarriorSkillRequirementType::BestClearTime },
	};
	if (const EWarriorSkillRequirementType* Found = Types.Find(InServerType))
	{
		OutType = *Found;
		return true;
	}
	return false;
}

FText UWarriorSkillUnlockSubsystem::ErrorToText(const FString& InCode, int32 InStatus)
{
	if (InCode == WarriorSkillUnlockApi::RequirementsNotMet)
	{
		return LOCTEXT("RequirementsNotMet", "해금 조건을 채우지 못했습니다.");
	}
	if (InCode == TEXT("SKILL_ALREADY_UNLOCKED"))
	{
		return LOCTEXT("AlreadyUnlocked", "이미 해금한 스킬입니다.");
	}
	if (InCode == TEXT("NOT_LOGGED_IN"))
	{
		return LOCTEXT("NotLoggedIn", "로그인이 필요합니다.");
	}
	if (InCode == TEXT("NETWORK_ERROR") || InStatus >= 500 || InCode == TEXT("REQUEST_IN_PROGRESS") || InCode == TEXT("VERSION_CONFLICT"))
	{
		return LOCTEXT("TryLater", "잠시 뒤 다시 시도해 주세요.");
	}
	if (InStatus == 401)
	{
		return LOCTEXT("SessionEnded", "로그인이 끝났습니다. 다시 로그인해 주세요.");
	}
	//SKILL_UNKNOWN·VALIDATION_FAILED·IDEMPOTENCY_KEY_REUSED 등: 게임 쪽 버그
	return LOCTEXT("CannotProcess", "요청을 처리할 수 없습니다.");
}

#undef LOCTEXT_NAMESPACE
