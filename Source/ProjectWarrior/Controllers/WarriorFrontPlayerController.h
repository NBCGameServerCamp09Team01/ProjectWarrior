// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorPlayerController.h"
#include "WarriorFrontPlayerController.generated.h"

class UUserWidget;

//프론트 레벨의 화면 단계. 로그인 화면이 생기면 Title과 MainMenu 사이에 추가한다.
UENUM(BlueprintType)
enum class EWarriorFrontScreen : uint8
{
	None,
	Title,
	MainMenu,
	Growth,		// 성장 화면(스탯 투자, 스킬 해금). 메인메뉴에서 연다
	Skill		// 스킬 화면(스킬 해금). 메인메뉴에서 연다
};

/**
 * 타이틀·메인메뉴 레벨의 PlayerController.
 * 화면(위젯) 전환과 스테이지 시작·종료를 맡는다. 위젯의 버튼은 이 클래스 함수만 호출한다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorFrontPlayerController : public AWarriorPlayerController
{
	GENERATED_BODY()

public:
	//현재 화면을 숨기고 InScreen 위젯을 띄운다. 위젯은 처음 보여 줄 때 한 번만 만든다.
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void ShowScreen(EWarriorFrontScreen InScreen);

	//StageLevel로 이동한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void StartStage();

	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void QuitGame();

	UFUNCTION(BlueprintPure, Category = "Warrior|Front")
	EWarriorFrontScreen GetCurrentScreen() const { return CurrentScreen; }

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//화면별 위젯 클래스 (BP_FrontPlayerController에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front")
	TMap<EWarriorFrontScreen, TSubclassOf<UUserWidget>> ScreenWidgetClasses;

	//"스테이지 시작"으로 이동할 레벨 (BP에서 L_Stage_Test 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front")
	TSoftObjectPtr<UWorld> StageLevel;

private:
	//화면이 바뀐 뒤 음악 상황을 알리고 전환 소리를 낸다(어떤 소리인지는 사운드 표가 정함)
	void PlayScreenSound(EWarriorFrontScreen InPreviousScreen, EWarriorFrontScreen InNextScreen);

	//한 번 만든 화면 위젯을 보관해 다시 보여 줄 때 재사용
	UPROPERTY(Transient)
	TMap<EWarriorFrontScreen, TObjectPtr<UUserWidget>> ScreenWidgets;

	EWarriorFrontScreen CurrentScreen = EWarriorFrontScreen::None;
};
