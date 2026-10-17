// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorErrorText.generated.h"

//문구를 고를 때 쓰는 화면 맥락. 같은 코드라도 화면마다 문구가 다를 때만 쓴다
namespace WarriorErrorContext
{
	inline const FName Login = TEXT("Login");
	inline const FName Signup = TEXT("Signup");
	//채팅 창(M23). 길드 없음 등을 팝업이 아니라 채팅 창 안 문구로
	inline const FName Chat = TEXT("Chat");
	//길드 팝업(M23). 길드 요청은 모두 이 맥락으로 보낸다(UWarriorGuildSubsystem)
	inline const FName Guild = TEXT("Guild");
}

/**
 * 오류 문구 표(S5 UF-4). 오류 코드 → 문구·보여 주는 방법·다음 행동.
 * 표는 WarriorErrorText.cpp 한 곳에 있다. 새 오류 코드를 더할 때는 그 파일의 해당 구역에 한 줄을 더한다.
 *   공통·인증·STAGE_·STAGE_PLAY_·RESULT_는 b, STAT_·SKILL_은 c, RANK_는 d가 더한다.
 *
 * 찾는 순서: ① (맥락, 코드) 덮어쓰기 ② 코드 ③ 접두어 기본값(AUTH_, STAGE_ 등) ④ HTTP 상태(0·401·5xx) ⑤ 알 수 없는 오류(맥락 → 공통).
 * 문구 안의 {Code}는 오류 코드로 바뀐다.
 */
struct PROJECTWARRIOR_API FWarriorErrorText
{
	//오류 하나에 맞는 규칙을 찾는다. 돌려준 규칙의 Code는 InError.Code(비어 있으면 찾은 줄의 코드)
	static FWarriorErrorRule Resolve(const FWarriorApiError& InError, FName InContext = NAME_None);

	//표에서 코드 한 줄을 찾는다. 없으면 nullptr(접두어·상태 기본값은 보지 않는다)
	static const FWarriorErrorRule* Find(const FString& InCode);

	//코드의 문구만. 표에 없으면 알 수 없는 오류 문구
	static FText GetText(const FString& InCode, FName InContext = NAME_None);

	//표 전체(자동 시험용)
	static const TArray<FWarriorErrorRule>& GetAllRules();
};

//BP에서 문구를 꺼낼 때
UCLASS()
class PROJECTWARRIOR_API UWarriorErrorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//오류 코드의 화면 문구(예: STAGE_PLAY_EXPIRED)
	UFUNCTION(BlueprintPure, Category = "Warrior|Api")
	static FText GetErrorText(const FString& InCode);
};
