// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WarriorTutorialTypes.generated.h"

class UDataAsset_Item;
class UInputAction;

//튜토리얼 진행 상태(계획서 4-2). 전환은 AWarriorTutorialGameMode
UENUM(BlueprintType)
enum class EWarriorTutorialState : uint8
{
	Loading,	// 플레이어 possess를 기다림
	StepIntro,	// 단계 준비(차단·아이템·구역) 후 문구를 잠깐 보임
	StepActive,	// 완료 신호를 기다림
	StepDone,	// 완료 연출
	Finished	// 마지막 장까지 끝남. 완료 화면
};

//단계를 끝내는 신호의 종류. 어빌리티를 고치지 않고 듣는다(계획서 2장 B·C)
UENUM(BlueprintType)
enum class EWarriorTutorialGoal : uint8
{
	//GoalTag(부모 포함)를 자기 태그로 가진 어빌리티가 켜짐 (ASC AbilityActivatedCallbacks)
	AbilityActivated,
	//GoalTag 게임플레이 이벤트를 받음 (ASC GenericGameplayEventCallbacks). 예: Player.Event.Successful.Block
	GameplayEvent,
	//GoalTag 상태 태그가 새로 붙음 (ASC RegisterGameplayTagEvent). 예: Player.Status.TargetLock
	TagAdded,
	//단계 구역(AWarriorTutorialZone)의 도착 볼륨에 들어감
	ReachZone,
	//KeyAction 입력이 RequiredInputSeconds만큼 들어옴. 0이면 한 번 누르면 끝(설명 카드의 "계속")
	InputUsed,
	//이 단계에서 준 아이템을 씀(인벤토리에서 개수가 줄어듦)
	ItemUsed
};

//단계 시작 때 주는 아이템
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorTutorialItemGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	TObjectPtr<UDataAsset_Item> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "1"))
	int32 Count = 1;
};

//튜토리얼 한 단계
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorTutorialStep
{
	GENERATED_BODY()

	//로그·구역 이름에 쓰는 이름(예: Block). 장 안에서 겹치지 않게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	FName StepId;

	//할 일. {Key}는 KeyAction에 지금 매핑된 키(없으면 FallbackKeyText)로 바뀐다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (MultiLine = "true"))
	FText Instruction;

	//키를 보일 입력. 비우면 키 칸을 숨긴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	TObjectPtr<UInputAction> KeyAction;

	//매핑된 키를 찾지 못하거나 키로 나타낼 수 없는 조작(예: "이동 키 두 번")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	FText FallbackKeyText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	EWarriorTutorialGoal Goal = EWarriorTutorialGoal::AbilityActivated;

	//AbilityActivated·GameplayEvent·TagAdded가 쓰는 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial",
		meta = (EditCondition = "Goal == EWarriorTutorialGoal::AbilityActivated || Goal == EWarriorTutorialGoal::GameplayEvent || Goal == EWarriorTutorialGoal::TagAdded"))
	FGameplayTag GoalTag;

	//신호가 올 때 플레이어가 함께 갖고 있어야 하는 태그(예: 패링 = Successful.Block + Player.Status.Blocking.Perfect). 비우면 보지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	FGameplayTag RequiredOwnedTag;

	//몇 번 해야 끝나는가
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	//InputUsed: 입력이 들어온 시간의 합(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial",
		meta = (EditCondition = "Goal == EWarriorTutorialGoal::InputUsed", ClampMin = "0.0", Units = "s"))
	float RequiredInputSeconds = 0.f;

	//실패로 세는 이벤트(예: 플레이어 피격 Shared.Event.HitReact). 비우면 세지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	FGameplayTag FailureEventTag;

	//이만큼 실패하면 힌트를 보이고 힌트 슬로우를 켠다. 0이면 힌트 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "0"))
	int32 HintAfterFailures = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (MultiLine = "true"))
	FText HintText;

	//이 단계부터 쓸 수 있는 어빌리티(자기 태그, 부모 태그면 그 아래 전부). 앞 단계에서 푼 것은 그대로 풀려 있다(계획서 2장 G)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (Categories = "Player.Ability"))
	FGameplayTagContainer UnlockAbilityTags;

	//단계 시작 때 주는 아이템
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	TArray<FWarriorTutorialItemGrant> GrantItems;

	//레벨의 AWarriorTutorialZone 이름. 단계 시작 때 그 구역으로 옮기고 적을 부른다. 비우면 그대로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	FName ZoneId;
};

//튜토리얼 한 장(메인메뉴의 장 고르기 단위)
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorTutorialChapter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (TitleProperty = "StepId"))
	TArray<FWarriorTutorialStep> Steps;
};
