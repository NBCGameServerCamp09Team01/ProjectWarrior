// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorPlayerController.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "WarriorFrontPlayerController.generated.h"

class UUserWidget;
class UWarriorNoticePopupWidget;

//프론트 레벨의 화면 단계. 흐름은 Title → (로그인 안 했으면 Login ↔ Signup) → MainMenu.
//BP에 저장된 값이 바뀌지 않도록 새 값은 끝에 추가한다.
UENUM(BlueprintType)
enum class EWarriorFrontScreen : uint8
{
	None,
	Title,
	MainMenu,
	Growth,		// 성장 화면(스탯 투자, 스킬 해금). 메인메뉴에서 연다
	Skill,		// 스킬 화면(스킬 해금). 메인메뉴에서 연다
	Login,		// 로그인 화면. 로그인하지 않은 채 MainMenu를 열면 대신 뜬다
	Signup		// 회원가입 화면. 로그인 화면에서 연다
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
	//로그인하지 않았으면 MainMenu 대신 Login을 띄운다.
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void ShowScreen(EWarriorFrontScreen InScreen);

	//StageLevel로 이동한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void StartStage();

	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void QuitGame();

	//로그아웃한다(메인메뉴의 로그아웃 버튼). 화면 이동은 OnSessionEnded를 받아 HandleSessionEnded가 한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void Logout();

	UFUNCTION(BlueprintPure, Category = "Warrior|Front")
	EWarriorFrontScreen GetCurrentScreen() const { return CurrentScreen; }

	//현재 화면 위에 안내 팝업을 띄운다. 이미 떠 있으면 문구만 바꾼다. NoticePopupClass가 없으면 로그만 남긴다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void ShowNotice(const FText& InMessage);

protected:
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//화면별 위젯 클래스 (BP_FrontPlayerController에서 지정)
	//Login → WBP_Login, Signup → WBP_Signup
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front")
	TMap<EWarriorFrontScreen, TSubclassOf<UUserWidget>> ScreenWidgetClasses;

	//"스테이지 시작"으로 이동할 레벨 (BP에서 L_Stage_Test 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front")
	TSoftObjectPtr<UWorld> StageLevel;

	//안내 팝업 (BP_FrontPlayerController에서 WBP_NoticePopup 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front")
	TSubclassOf<UWarriorNoticePopupWidget> NoticePopupClass;

private:
	//로그인 상태가 끝났을 때(UWarriorAuthSubsystem::OnSessionEnded). ApplySessionEnd로 넘긴다
	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage);

	//이유에 맞는 화면으로 옮기고, 문구가 있으면 안내 팝업을 띄운다.
	//LoggedOut·Replaced·ConnectionLost → 타이틀, Expired·InvalidToken → 로그인 화면 (auth-api.md "게임 쪽 처리")
	//알림을 직접 받았을 때와, 스테이지에서 끊겨 이 레벨로 돌아왔을 때(BeginPlay) 함께 쓴다
	void ApplySessionEnd(EWarriorSessionEndReason InReason, const FText& InMessage);

	//안내 팝업이 닫히면 아래 화면으로 입력 포커스를 돌려준다
	UFUNCTION()
	void HandleNoticeClosed();

	//화면이 바뀐 뒤 음악 상황을 알리고 전환 소리를 낸다(어떤 소리인지는 사운드 표가 정함)
	void PlayScreenSound(EWarriorFrontScreen InPreviousScreen, EWarriorFrontScreen InNextScreen);

	//한 번 만든 화면 위젯을 보관해 다시 보여 줄 때 재사용
	UPROPERTY(Transient)
	TMap<EWarriorFrontScreen, TObjectPtr<UUserWidget>> ScreenWidgets;

	//한 번 만든 안내 팝업을 재사용
	UPROPERTY(Transient)
	TObjectPtr<UWarriorNoticePopupWidget> NoticePopup;

	EWarriorFrontScreen CurrentScreen = EWarriorFrontScreen::None;
};
