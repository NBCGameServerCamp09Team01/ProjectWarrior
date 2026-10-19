// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WarriorChatSettings.generated.h"

class UUserWidget;

/**
 * 프로젝트 설정 > Game > Warrior Chat. 채팅(M23)의 화면 값을 모은다. 값은 Config/DefaultGame.ini에 저장된다.
 * 채팅 창 클래스를 이진 에셋(WBP_MainMenu·컨트롤러 BP)이 아니라 여기에 적어, 메인메뉴 에셋을 같이 고치지 않고 채팅 창을 바꾼다(채팅 계획서 C-7).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Warrior Chat"))
class PROJECTWARRIOR_API UWarriorChatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UWarriorChatSettings* Get() { return GetDefault<UWarriorChatSettings>(); }

	//메인메뉴의 채팅 자리(Slot_Chat)에 넣을 채팅 창 위젯(예: WBP_ChatPanel). 비어 있거나 불러오지 못하면 채팅 창 없이 메인메뉴만 뜬다
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> ChatPanelClass;
};
