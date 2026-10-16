// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorChatTypes.generated.h"

/**
 * 채팅(M23) 자료형. 명세: 루트 docs/contracts/chat-api.md "채팅 한 줄", realtime-api.md CHAT_MESSAGE
 */

//채팅 탭. BP에 저장될 수 있으므로 새 값은 끝에 더한다
UENUM(BlueprintType)
enum class EWarriorChatScope : uint8
{
	Global,		// 전체(접속한 모두)
	Guild		// 내 길드
};

//채팅 한 줄(서버가 매긴 것). CH1·CH2 응답, CH3 내역, 실시간 CHAT_MESSAGE의 data가 모두 이 모양이다
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorChatMessage
{
	GENERATED_BODY()

	//메시지 번호. 모양(숫자·UUID)에 기대지 않는다 — 같은지 비교와 내역 커서에만 쓴다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FString MessageId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	EWarriorChatScope Scope = EWarriorChatScope::Global;

	//길드 채팅이면 길드 ID(10진수 문자열), 전체면 빈 값
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FString GuildId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FString SenderAccountId;

	//보낸 때의 닉네임
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FString SenderNickname;

	//글(서버가 앞뒤 공백을 자른 것). 서식 없이 그린다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FString Text;

	//서버가 받은 시각(UTC). 화면은 PC 시간대로 바꿔 HH:MM
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FDateTime SentAt;
};

//채팅 창의 한 줄 상태. 내 글은 응답이 올 때까지 Sending으로 먼저 보인다
UENUM(BlueprintType)
enum class EWarriorChatLineState : uint8
{
	Sent,		// 서버가 받은 줄(내 글·남의 글·내역)
	Sending,	// 내가 보내는 중(번호 없음)
	Failed		// 보내지 못함 — 같은 requestId로 [다시]
};

//채팅 창의 한 줄
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorChatLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FWarriorChatMessage Message;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	EWarriorChatLineState State = EWarriorChatLineState::Sent;

	//내가 보낸 줄의 requestId(Sending·Failed일 때 [다시]에 씀). 남의 줄은 빈 값
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Chat")
	FString RequestId;
};

//보내기 전에 게임이 먼저 보는 글 규칙의 결과(최종 판정은 서버, chat-api.md "규칙")
UENUM(BlueprintType)
enum class EWarriorChatTextCheck : uint8
{
	Ok,
	Empty,			// 공백만
	TooLong,		// 코드 포인트 100자 넘음
	ControlChar		// 줄바꿈·탭 같은 제어 문자
};

//SendMessage의 결과. Sending만 실제로 보냈다
UENUM(BlueprintType)
enum class EWarriorChatSendResult : uint8
{
	Sending,
	Empty,
	TooLong,
	ControlChar,
	Blocked,		// 429 남은 시간 동안 보내기 막힘
	NotLoggedIn,
	NotSent			// 요청을 시작하지 못함(로그)
};

//길드 탭이 아는 내 길드 상태. U2(UWarriorGuildSubsystem) 전에는 서버 응답으로 안다
UENUM(BlueprintType)
enum class EWarriorChatGuildState : uint8
{
	Unknown,	// 아직 모름(길드 탭을 열면 CH3로 확인)
	None,		// 길드 없음(CH2·CH3가 403 GUILD_NOT_MEMBER)
	Member
};
