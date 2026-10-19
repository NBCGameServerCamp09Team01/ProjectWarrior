// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorStageListSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Auth/WarriorAuthSubsystem.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "ProjectWarrior/Stage/WarriorStageLaunchSubsystem.h"

namespace WarriorStageListApi
{
	const TCHAR* StagesPath = TEXT("/stages");
	const TCHAR* MyStagesPath = TEXT("/accounts/me/stages");

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

	//null이거나 없으면 빈 문자열(requires·firstClearedAt은 칸은 늘 있고 값이 null일 수 있다)
	FString GetStringOrEmpty(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		FString Value;
		return InObject->TryGetStringField(InField, Value) ? Value : FString();
	}

	int32 GetIntOrZero(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InField)
	{
		int32 Value = 0;
		return InObject->TryGetNumberField(InField, Value) ? Value : 0;
	}
}

UWarriorStageListSubsystem* UWarriorStageListSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorStageListSubsystem>() : nullptr;
}

void UWarriorStageListSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWarriorAuthSubsystem* Auth = Collection.InitializeDependency<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.AddUniqueDynamic(this, &ThisClass::HandleSessionEnded);
	}
}

void UWarriorStageListSubsystem::Deinitialize()
{
	if (UWarriorAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UWarriorAuthSubsystem>())
	{
		Auth->OnSessionEnded.RemoveDynamic(this, &ThisClass::HandleSessionEnded);
	}

	Super::Deinitialize();
}

const FWarriorStageListEntry* UWarriorStageListSubsystem::FindStage(FName InStageId) const
{
	for (const FWarriorRegionListEntry& Region : Regions)
	{
		for (const FWarriorStageListEntry& Stage : Region.Stages)
		{
			if (Stage.StageId == InStageId)
			{
				return &Stage;
			}
		}
	}
	return nullptr;
}

bool UWarriorStageListSubsystem::HasDifficulty(int32 InDifficulty) const
{
	return Difficulties.ContainsByPredicate(
		[InDifficulty](const FWarriorDifficultyInfo& Info) { return Info.Difficulty == InDifficulty; });
}

FText UWarriorStageListSubsystem::GetDifficultyDisplayName(int32 InDifficulty) const
{
	const FWarriorDifficultyInfo* Info = Difficulties.FindByPredicate(
		[InDifficulty](const FWarriorDifficultyInfo& Entry) { return Entry.Difficulty == InDifficulty; });
	return Info ? Info->DisplayName : FText::AsNumber(InDifficulty);
}

void UWarriorStageListSubsystem::RequestStageList()
{
	const int32 Serial = ++RequestSerial;
	bRequesting = true;

	//정의는 서버가 다시 뜰 때까지 바뀌지 않으므로 한 번 받아 둔다(명세 ST1)
	if (bHasDefinitions)
	{
		SendProgress(Serial);
	}
	else
	{
		SendDefinitions(Serial);
	}
}

void UWarriorStageListSubsystem::SendDefinitions(int32 InSerial)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);

	//ST1은 인증이 필요 없지만, 선택 팝업은 로그인 뒤에만 열리므로 같은 요청 길을 쓴다
	const bool bStarted = Auth && Auth->SendAuthorized(TEXT("GET"), WarriorStageListApi::StagesPath, FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InSerial](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleDefinitionsResponse(InSerial, Status, Body);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StageList] Definitions request not sent (not logged in)."));
		Fail(InSerial, 401, FString());
	}
}

void UWarriorStageListSubsystem::SendProgress(int32 InSerial)
{
	UWarriorAuthSubsystem* Auth = UWarriorAuthSubsystem::Get(this);

	const bool bStarted = Auth && Auth->SendAuthorized(TEXT("GET"), WarriorStageListApi::MyStagesPath, FString(),
		[WeakThis = TWeakObjectPtr<ThisClass>(this), InSerial](int32 Status, const FString& Body)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleProgressResponse(InSerial, Status, Body);
			}
		});

	if (!bStarted)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[StageList] Progress request not sent (not logged in)."));
		Fail(InSerial, 401, FString());
	}
}

void UWarriorStageListSubsystem::HandleDefinitionsResponse(int32 InSerial, int32 Status, const FString& Body)
{
	if (!IsCurrentRequest(InSerial))
	{
		return;
	}

	if (Status != 200)
	{
		Fail(InSerial, Status, Body);
		return;
	}

	if (!ParseDefinitions(Body))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StageList] Definitions response could not be read."));
		Fail(InSerial, -1, FString());
		return;
	}

	bHasDefinitions = true;
	SendProgress(InSerial);
}

void UWarriorStageListSubsystem::HandleProgressResponse(int32 InSerial, int32 Status, const FString& Body)
{
	if (!IsCurrentRequest(InSerial))
	{
		return;
	}

	if (Status != 200)
	{
		Fail(InSerial, Status, Body);
		return;
	}

	if (!ParseProgress(Body))
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[StageList] Progress response could not be read."));
		Fail(InSerial, -1, FString());
		return;
	}

	RebuildRegions();
	bRequesting = false;
	bHasList = true;
	OnStageListUpdated.Broadcast();
}

bool UWarriorStageListSubsystem::ParseDefinitions(const FString& InBody)
{
	using namespace WarriorStageListApi;

	const TSharedPtr<FJsonObject> Data = ReadData(InBody);
	const TArray<TSharedPtr<FJsonValue>>* RegionValues = nullptr;
	if (!Data.IsValid() || !Data->TryGetArrayField(TEXT("regions"), RegionValues))
	{
		return false;
	}

	//M24: 몬스터 이름(미리보기). 없으면(v1.3 전 서버) 키를 그대로 보인다
	TMap<FString, FString> MonsterNames;
	const TArray<TSharedPtr<FJsonValue>>* MonsterValues = nullptr;
	if (Data->TryGetArrayField(TEXT("monsters"), MonsterValues))
	{
		for (const TSharedPtr<FJsonValue>& MonsterValue : *MonsterValues)
		{
			const TSharedPtr<FJsonObject> MonsterObject = MonsterValue.IsValid() ? MonsterValue->AsObject() : nullptr;
			if (MonsterObject.IsValid())
			{
				MonsterNames.Add(GetStringOrEmpty(MonsterObject, TEXT("monsterId")), GetStringOrEmpty(MonsterObject, TEXT("name")));
			}
		}
	}

	//M9: 난이도 목록. 없으면(v1.4 전 서버) 비어 있다(선택 화면은 난이도 0만)
	TArray<FWarriorDifficultyInfo> NewDifficulties;
	const TArray<TSharedPtr<FJsonValue>>* DifficultyValues = nullptr;
	if (Data->TryGetArrayField(TEXT("difficulties"), DifficultyValues))
	{
		for (const TSharedPtr<FJsonValue>& DifficultyValue : *DifficultyValues)
		{
			const TSharedPtr<FJsonObject> DifficultyObject = DifficultyValue.IsValid() ? DifficultyValue->AsObject() : nullptr;
			if (DifficultyObject.IsValid())
			{
				FWarriorDifficultyInfo& Difficulty = NewDifficulties.AddDefaulted_GetRef();
				Difficulty.Difficulty = GetIntOrZero(DifficultyObject, TEXT("difficulty"));
				Difficulty.DisplayName = FText::FromString(GetStringOrEmpty(DifficultyObject, TEXT("name")));
			}
		}
	}

	TArray<FRegionDef> NewRegions;
	for (const TSharedPtr<FJsonValue>& RegionValue : *RegionValues)
	{
		const TSharedPtr<FJsonObject> RegionObject = RegionValue.IsValid() ? RegionValue->AsObject() : nullptr;
		if (!RegionObject.IsValid())
		{
			continue;
		}

		FRegionDef& Region = NewRegions.AddDefaulted_GetRef();
		Region.RegionId = FName(*GetStringOrEmpty(RegionObject, TEXT("regionId")));
		Region.Name = GetStringOrEmpty(RegionObject, TEXT("name"));
		Region.Order = GetIntOrZero(RegionObject, TEXT("order"));

		const TArray<TSharedPtr<FJsonValue>>* StageValues = nullptr;
		if (RegionObject->TryGetArrayField(TEXT("stages"), StageValues))
		{
			for (const TSharedPtr<FJsonValue>& StageValue : *StageValues)
			{
				const TSharedPtr<FJsonObject> StageObject = StageValue.IsValid() ? StageValue->AsObject() : nullptr;
				if (!StageObject.IsValid())
				{
					continue;
				}

				FStageDef& Stage = Region.Stages.AddDefaulted_GetRef();
				Stage.StageId = FName(*GetStringOrEmpty(StageObject, TEXT("stageId")));
				Stage.Name = GetStringOrEmpty(StageObject, TEXT("name"));
				Stage.Order = GetIntOrZero(StageObject, TEXT("order"));
				const FString Requires = GetStringOrEmpty(StageObject, TEXT("requires"));
				Stage.RequiresStageId = Requires.IsEmpty() ? NAME_None : FName(*Requires);
				Stage.WaveCount = GetIntOrZero(StageObject, TEXT("waveCount"));
				ParseWaves(StageObject, MonsterNames, Stage.Waves);
				ParseMultipliers(StageObject, Stage.Multipliers);
			}
		}

		Region.Stages.Sort([](const FStageDef& A, const FStageDef& B) { return A.Order < B.Order; });
	}

	NewRegions.Sort([](const FRegionDef& A, const FRegionDef& B) { return A.Order < B.Order; });
	RegionDefs = MoveTemp(NewRegions);
	Difficulties = MoveTemp(NewDifficulties);
	return true;
}

bool UWarriorStageListSubsystem::ParseProgress(const FString& InBody)
{
	using namespace WarriorStageListApi;

	const TSharedPtr<FJsonObject> Data = ReadData(InBody);
	const TArray<TSharedPtr<FJsonValue>>* StageValues = nullptr;
	if (!Data.IsValid() || !Data->TryGetArrayField(TEXT("stages"), StageValues))
	{
		return false;
	}

	TMap<FName, FStageProgress> NewProgress;
	for (const TSharedPtr<FJsonValue>& StageValue : *StageValues)
	{
		const TSharedPtr<FJsonObject> StageObject = StageValue.IsValid() ? StageValue->AsObject() : nullptr;
		if (!StageObject.IsValid())
		{
			continue;
		}

		const FName StageId(*GetStringOrEmpty(StageObject, TEXT("stageId")));
		const FString StatusText = GetStringOrEmpty(StageObject, TEXT("status"));

		FStageProgress& Entry = NewProgress.Add(StageId);
		if (StatusText == TEXT("OPEN"))
		{
			Entry.Status = EWarriorStageEntryStatus::Open;
		}
		else if (StatusText == TEXT("CLEARED"))
		{
			Entry.Status = EWarriorStageEntryStatus::Cleared;
		}
		else if (StatusText != TEXT("LOCKED"))
		{
			//모르는 상태(서버가 나중에 더한 값)는 잠김으로 그린다. 새 상태는 EWarriorStageEntryStatus 끝에 더한다
			UE_LOG(LogProjectWarrior, Warning, TEXT("[StageList] Unknown status '%s' for %s. Shown as locked."), *StatusText, *StageId.ToString());
		}

		const FString FirstClearedAt = GetStringOrEmpty(StageObject, TEXT("firstClearedAt"));
		Entry.bHasFirstCleared = !FirstClearedAt.IsEmpty() && FDateTime::ParseIso8601(*FirstClearedAt, Entry.FirstClearedAt);
	}

	Progress = MoveTemp(NewProgress);
	return true;
}

void UWarriorStageListSubsystem::RebuildRegions()
{
	TArray<FWarriorRegionListEntry> NewRegions;
	for (const FRegionDef& RegionDef : RegionDefs)
	{
		FWarriorRegionListEntry& Region = NewRegions.AddDefaulted_GetRef();
		Region.RegionId = RegionDef.RegionId;
		Region.DisplayName = FText::FromString(RegionDef.Name);
		Region.Order = RegionDef.Order;

		for (const FStageDef& StageDef : RegionDef.Stages)
		{
			//입장할 레벨이 UE 스테이지 목록에 없으면 줄을 뺀다(실제 구현 결정 D-7). "업데이트 필요" 모양은 확장 상태로 남겨 둔다
			FWarriorStageLaunchInfo LaunchInfo;
			if (!UWarriorStageLaunchSubsystem::BuildLaunchInfo(StageDef.StageId, LaunchInfo))
			{
				UE_LOG(LogProjectWarrior, Warning, TEXT("[StageList] Stage %s is on the server but not in the stage catalog. Hidden."), *StageDef.StageId.ToString());
				continue;
			}

			FWarriorStageListEntry& Stage = Region.Stages.AddDefaulted_GetRef();
			Stage.StageId = StageDef.StageId;
			Stage.RegionId = RegionDef.RegionId;
			Stage.DisplayName = FText::FromString(StageDef.Name);
			Stage.Order = StageDef.Order;
			Stage.RequiresStageId = StageDef.RequiresStageId;
			Stage.WaveCount = StageDef.WaveCount;

			if (const FStageProgress* StageProgress = Progress.Find(StageDef.StageId))
			{
				Stage.Status = StageProgress->Status;
				Stage.FirstClearedAt = StageProgress->FirstClearedAt;
				Stage.bHasFirstCleared = StageProgress->bHasFirstCleared;
			}
			Stage.bCanEnter = Stage.Status != EWarriorStageEntryStatus::Locked;

			if (Stage.Status == EWarriorStageEntryStatus::Cleared)
			{
				++Region.ClearedCount;
			}

			BuildWaveInfo(StageDef, Stage.Waves);
			Stage.Multipliers = StageDef.Multipliers;
		}

		Region.bCanOpen = IsRegionOpen(Region);
	}

	Regions = MoveTemp(NewRegions);

	int32 StageCount = 0;
	for (const FWarriorRegionListEntry& Region : Regions)
	{
		StageCount += Region.Stages.Num();
	}
	UE_LOG(LogProjectWarrior, Log, TEXT("[StageList] Stage list updated. Regions %d, stages %d"), Regions.Num(), StageCount);
}

void UWarriorStageListSubsystem::Fail(int32 InSerial, int32 Status, const FString& Body)
{
	if (!IsCurrentRequest(InSerial))
	{
		return;
	}

	bRequesting = false;

	FWarriorApiError Error;
	if (Status == -1)
	{
		Error.Code = TEXT("INVALID_RESPONSE");
	}
	else
	{
		Error = WarriorStageListApi::ParseError(Status, Body);
	}

	UE_LOG(LogProjectWarrior, Warning, TEXT("[StageList] Stage list request failed. status=%d code=%s retryable=%s"),
		Status, *Error.Code, Error.Retryable ? TEXT("true") : TEXT("false"));

	OnStageListFailed.Broadcast(Error.Code, Error.Message);
}

bool UWarriorStageListSubsystem::IsRegionOpen(const FWarriorRegionListEntry& InRegion)
{
	return true;
}

void UWarriorStageListSubsystem::BuildWaveInfo(const FStageDef& InStageDef, TArray<FWarriorStageWaveInfo>& OutWaves)
{
	//출처: 서버 스테이지 목록(ST1)의 지금 정의. 실제 판의 구성은 판 시작 때 서버가 고정한 값이라 다를 수 있다(waveVersion)
	OutWaves = InStageDef.Waves;
}

void UWarriorStageListSubsystem::ParseWaves(const TSharedPtr<FJsonObject>& InStageObject, const TMap<FString, FString>& InMonsterNames, TArray<FWarriorStageWaveInfo>& OutWaves)
{
	using namespace WarriorStageListApi;

	OutWaves.Reset();
	const TArray<TSharedPtr<FJsonValue>>* WaveValues = nullptr;
	if (!InStageObject.IsValid() || !InStageObject->TryGetArrayField(TEXT("waves"), WaveValues))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& WaveValue : *WaveValues)
	{
		const TSharedPtr<FJsonObject> WaveObject = WaveValue.IsValid() ? WaveValue->AsObject() : nullptr;
		if (!WaveObject.IsValid())
		{
			continue;
		}

		FWarriorStageWaveInfo& Wave = OutWaves.AddDefaulted_GetRef();
		Wave.WaveNumber = GetIntOrZero(WaveObject, TEXT("waveNumber"));
		WaveObject->TryGetBoolField(TEXT("bossWave"), Wave.bBossWave);

		const TArray<TSharedPtr<FJsonValue>>* EnemyValues = nullptr;
		if (!WaveObject->TryGetArrayField(TEXT("enemies"), EnemyValues))
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& EnemyValue : *EnemyValues)
		{
			const TSharedPtr<FJsonObject> EnemyObject = EnemyValue.IsValid() ? EnemyValue->AsObject() : nullptr;
			if (!EnemyObject.IsValid())
			{
				continue;
			}

			FWarriorStageWaveEnemyInfo& Enemy = Wave.Enemies.AddDefaulted_GetRef();
			Enemy.EnemyKey = GetStringOrEmpty(EnemyObject, TEXT("monsterId"));
			const FString* Name = InMonsterNames.Find(Enemy.EnemyKey);
			Enemy.DisplayName = FText::FromString(Name && !Name->IsEmpty() ? *Name : Enemy.EnemyKey);
			Enemy.Count = GetIntOrZero(EnemyObject, TEXT("count"));
			EnemyObject->TryGetBoolField(TEXT("boss"), Enemy.bBoss);

			//M9: 난이도별 마리 수(서버 계산값). 없으면 Count만
			const TArray<TSharedPtr<FJsonValue>>* CountValues = nullptr;
			if (EnemyObject->TryGetArrayField(TEXT("counts"), CountValues))
			{
				for (const TSharedPtr<FJsonValue>& CountValue : *CountValues)
				{
					Enemy.Counts.Add(CountValue.IsValid() ? FMath::Max(0, static_cast<int32>(CountValue->AsNumber())) : 0);
				}
			}
		}
	}
}

void UWarriorStageListSubsystem::ParseMultipliers(const TSharedPtr<FJsonObject>& InStageObject, TArray<FWarriorStageDifficultyMultipliers>& OutMultipliers)
{
	OutMultipliers.Reset();
	const TArray<TSharedPtr<FJsonValue>>* MultiplierValues = nullptr;
	if (!InStageObject.IsValid() || !InStageObject->TryGetArrayField(TEXT("multipliers"), MultiplierValues))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& MultiplierValue : *MultiplierValues)
	{
		const TSharedPtr<FJsonObject> MultiplierObject = MultiplierValue.IsValid() ? MultiplierValue->AsObject() : nullptr;
		if (!MultiplierObject.IsValid())
		{
			continue;
		}

		FWarriorStageDifficultyMultipliers& Multipliers = OutMultipliers.AddDefaulted_GetRef();
		double Value = 0.0;
		Multipliers.Difficulty = MultiplierObject->TryGetNumberField(TEXT("difficulty"), Value) ? static_cast<int32>(Value) : OutMultipliers.Num() - 1;
		Multipliers.EnemyCountMultiplier = MultiplierObject->TryGetNumberField(TEXT("enemyCountMultiplier"), Value) ? static_cast<float>(Value) : 1.f;
		Multipliers.EnemyHealthMultiplier = MultiplierObject->TryGetNumberField(TEXT("enemyHealthMultiplier"), Value) ? static_cast<float>(Value) : 1.f;
		Multipliers.EnemyDamageMultiplier = MultiplierObject->TryGetNumberField(TEXT("enemyDamageMultiplier"), Value) ? static_cast<float>(Value) : 1.f;
		Multipliers.GoldAmountMultiplier = MultiplierObject->TryGetNumberField(TEXT("goldAmountMultiplier"), Value) ? static_cast<float>(Value) : 1.f;
	}
}

void UWarriorStageListSubsystem::HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage)
{
	//진행은 계정마다 다르므로 비운다. 정의(ST1)는 모두에게 같아 남긴다
	++RequestSerial;
	bRequesting = false;
	bHasList = false;
	Progress.Reset();
	Regions.Reset();

	//다음 계정이 앞 계정의 스테이지 선택값을 이어받지 않게 한다
	if (UWarriorStageLaunchSubsystem* LaunchSubsystem = GetGameInstance()->GetSubsystem<UWarriorStageLaunchSubsystem>())
	{
		LaunchSubsystem->ClearSelection();
	}
}
