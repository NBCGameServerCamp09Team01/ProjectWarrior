// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorAuthTypes.generated.h"

/**
 * 인증 API의 계정 본문(docs/contracts/auth-api.md "계정 본문").
 * 비밀번호와 토큰은 여기에 두지 않는다(토큰은 UWarriorAuthSubsystem만 보관한다).
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorAuthAccount
{
	GENERATED_BODY()

	//서버가 만든 계정 번호
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	int64 AccountId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FString LoginId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FString Nickname;

	//가입 시각(UTC)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Auth")
	FDateTime CreatedAt;
};
