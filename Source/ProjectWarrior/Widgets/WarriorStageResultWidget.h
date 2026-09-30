// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWarrior/Stage/WarriorStageTypes.h"
#include "WarriorStageResultWidget.generated.h"

/**
 * 스테이지 결과 화면의 베이스.
 * 배치와 글자 채우기는 WBP_StageResult가 하고(On Result Set), 버튼은 아래 Request 함수만 호출한다.
 * 위젯은 AWarriorStagePlayerController가 끝 상태(StageCleared·StageFailed)에서 만든다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorStageResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorStageResultWidget(const FObjectInitializer& ObjectInitializer);

	//결과를 저장하고 BP_OnResultSet을 호출한다
	void SetResult(const FWarriorStageResult& InResult);

	UFUNCTION(BlueprintPure, Category = "Warrior|Stage")
	const FWarriorStageResult& GetResult() const { return Result; }

protected:
	//결과가 들어오면 WBP에서 제목·통계 글자를 채운다
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Stage", meta = (DisplayName = "On Result Set"))
	void BP_OnResultSet(const FWarriorStageResult& InResult);

	//"다시 하기" 버튼용. 소유 컨트롤러의 RestartStage를 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RequestRestart();

	//"메인메뉴" 버튼용. 소유 컨트롤러의 ReturnToMainMenu를 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stage")
	void RequestReturnToMainMenu();

private:
	UPROPERTY(Transient)
	FWarriorStageResult Result;
};
