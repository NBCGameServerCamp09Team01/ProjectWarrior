// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGuildName.h"

bool FWarriorGuildName::IsNameLetter(TCHAR InChar)
{
	//한글 완성형 '가'(U+AC00) ~ '힣'(U+D7A3). 자모(ㄱ·ㅏ)는 넣지 않는다
	return (InChar >= 0xAC00 && InChar <= 0xD7A3)
		|| (InChar >= TEXT('A') && InChar <= TEXT('Z'))
		|| (InChar >= TEXT('a') && InChar <= TEXT('z'))
		|| (InChar >= TEXT('0') && InChar <= TEXT('9'));
}

FWarriorGuildNameCheck FWarriorGuildName::Check(const FString& InName)
{
	FWarriorGuildNameCheck Result;
	const int32 Length = InName.Len();
	Result.bLengthOk = Length >= MinLength && Length <= MaxLength;

	Result.bCharsOk = Length > 0;
	bool bDoubleSpace = false;
	for (int32 Index = 0; Index < Length; ++Index)
	{
		const TCHAR Char = InName[Index];
		if (Char == TEXT(' '))
		{
			bDoubleSpace |= Index > 0 && InName[Index - 1] == TEXT(' ');
		}
		else if (!IsNameLetter(Char))
		{
			Result.bCharsOk = false;
		}
	}

	//앞뒤 글자가 공백이 아니어야 한다(정규식의 첫 글자·끝 글자)
	Result.bSpacesOk = Length > 0 && InName[0] != TEXT(' ') && InName[Length - 1] != TEXT(' ') && !bDoubleSpace;
	return Result;
}

bool FWarriorGuildName::IsValidSearchPrefix(const FString& InPrefix)
{
	const int32 Length = InPrefix.Len();
	if (Length < 1 || Length > MaxLength || InPrefix[0] == TEXT(' '))
	{
		return false;
	}
	for (const TCHAR Char : InPrefix)
	{
		if (Char != TEXT(' ') && !IsNameLetter(Char))
		{
			return false;
		}
	}
	return true;
}
