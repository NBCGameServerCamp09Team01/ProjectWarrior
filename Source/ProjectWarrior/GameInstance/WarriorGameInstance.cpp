// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/ProjectWarrior.h"

UWarriorGameInstance* UWarriorGameInstance::Get(const UObject* WorldContextObject)
{
	return Cast<UWarriorGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
}

void UWarriorGameInstance::SelectStage(FName InStageId, int32 InDifficulty)
{
	const int32 NewDifficulty = FMath::Max(0, InDifficulty);
	if (SelectedStageId == InStageId && SelectedDifficulty == NewDifficulty)
	{
		return;
	}

	SelectedStageId = InStageId;
	SelectedDifficulty = NewDifficulty;

	UE_LOG(LogProjectWarrior, Log, TEXT("[GameInstance] Stage selected: %s, Difficulty %d"),
		*SelectedStageId.ToString(), SelectedDifficulty);

	OnStageSelectionChanged.Broadcast(SelectedStageId, SelectedDifficulty);
}

void UWarriorGameInstance::Init()
{
	Super::Init();

	//서브시스템은 Super::Init에서 이미 만들어져 있다. 이 로그가 나오면 GameInstance 클래스 설정이 적용된 것이다.
	UE_LOG(LogProjectWarrior, Log, TEXT("[GameInstance] Init (%s)"), *GetClass()->GetName());
}

void UWarriorGameInstance::Shutdown()
{
	UE_LOG(LogProjectWarrior, Log, TEXT("[GameInstance] Shutdown"));

	Super::Shutdown();
}
