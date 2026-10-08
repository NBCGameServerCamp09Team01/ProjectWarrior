// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectWarrior/Account/WarriorAccountTypes.h"
#include "WarriorMainMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * 메인메뉴의 베이스.
 * 계정 요약(계정 레벨, 남은 스탯 포인트)과 환영 문구를 표시하고, "성장" 버튼으로 성장 화면을, "로그아웃" 버튼으로 로그아웃을 한다.
 * 계정 값은 UWarriorAccountSubsystem(GameInstance 서브시스템)에서 읽고, 값이 바뀌면(레벨 업, 스탯 투자) 다시 표시한다.
 * 환영 문구의 닉네임은 UWarriorAuthSubsystem의 계정에서 읽는다(지금은 서버가 닉네임을 주지 않아 이름 없는 문구가 나온다).
 * 스테이지 시작·종료 버튼은 WBP_MainMenu가 AWarriorFrontPlayerController의 함수를 직접 호출한다.
 */
UCLASS(Abstract)
class PROJECTWARRIOR_API UWarriorMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWarriorMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	//계정 요약을 표시한다. 계정 상태가 바뀌면(레벨 업, 스탯 투자) 다시 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void SetAccountSummary(int32 InAccountLevel, int32 InStatPoints);

	//성장 화면을 연다. 소유 컨트롤러의 ShowScreen(Growth)를 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void RequestOpenGrowth();

	//스킬 화면을 연다. 소유 컨트롤러의 ShowScreen(Skill)을 호출한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Front")
	void RequestOpenSkill();

protected:
	//~ Begin UUserWidget Interface.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//화면이 보일 때마다 계정 요약을 다시 읽는다
	void RefreshAccountSummary();

	//화면이 보일 때마다 환영 문구를 다시 만든다. 닉네임이 있으면 WelcomeFormat, 없으면 WelcomeNoNameText
	void RefreshWelcome();

	UFUNCTION()
	void HandleGrowthButtonClicked();

	UFUNCTION()
	void HandleSkillButtonClicked();

	//소유 컨트롤러의 Logout을 부른다. 화면 이동은 컨트롤러가 OnSessionEnded를 받아 한다
	UFUNCTION()
	void HandleLogoutButtonClicked();

	//계정 서브시스템의 OnAccountChanged에 연결
	UFUNCTION()
	void HandleAccountChanged(const FWarriorAccountData& InAccountData);

	//~ Begin 없어도 되는 위젯 (WBP에 같은 이름으로 두면 연결된다)
	//"Lv. 3"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AccountLevel;

	//"스탯 포인트 2"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StatPoints;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Growth;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Skill;

	//"OOO 영웅님, 환영합니다."
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Welcome;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Logout;
	//~ End 없어도 되는 위젯

	//{0} 계정 레벨
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front|Text")
	FText AccountLevelFormat;

	//{0} 남은 스탯 포인트
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front|Text")
	FText StatPointsFormat;

	//{0} 닉네임
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front|Text")
	FText WelcomeFormat;

	//닉네임을 모를 때의 환영 문구. 로그인 아이디는 로그인 정보라 화면에 보이지 않는다
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Front|Text")
	FText WelcomeNoNameText;
};
