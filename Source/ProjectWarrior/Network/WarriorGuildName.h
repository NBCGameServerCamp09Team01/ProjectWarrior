// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 길드 이름 규칙(guild-api.md v1 "규칙"). 서버 GuildCreateRequest.NAME_PATTERN과 같은 규칙을 게임에서 먼저 본다.
 *   ^(?!.* {2})[가-힣A-Za-z0-9][가-힣A-Za-z0-9 ]{0,10}[가-힣A-Za-z0-9]$
 * 만들기 대화상자가 규칙을 하나씩 ✓로 보이도록 셋으로 나눈다. 서버가 최종 판정이고, 여기는 400을 줄이는 미리 보기다.
 * 한글은 완성형(U+AC00~U+D7A3)만. 정규식(ICU) 대신 글자를 하나씩 본다(범위를 서버와 정확히 맞추려고).
 */
struct PROJECTWARRIOR_API FWarriorGuildNameCheck
{
	//2~12자(공백 포함)
	bool bLengthOk = false;

	//한글 완성형·영문·숫자·공백만
	bool bCharsOk = false;

	//앞뒤 공백 없음, 연속 공백 없음
	bool bSpacesOk = false;

	bool IsValid() const { return bLengthOk && bCharsOk && bSpacesOk; }
};

struct PROJECTWARRIOR_API FWarriorGuildName
{
	static constexpr int32 MinLength = 2;
	static constexpr int32 MaxLength = 12;

	//만들기 이름 검사
	static FWarriorGuildNameCheck Check(const FString& InName);

	//길드 찾기 검색어(G1 name). 서버 GuildQueryService.NAME_PREFIX와 같다: 1~12자, 첫 글자는 공백이 아님, 쓸 수 있는 글자만
	static bool IsValidSearchPrefix(const FString& InPrefix);

	//한글 완성형·영문·숫자
	static bool IsNameLetter(TCHAR InChar);
};
