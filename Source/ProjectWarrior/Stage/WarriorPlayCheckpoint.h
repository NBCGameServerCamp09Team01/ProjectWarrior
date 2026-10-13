// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Stage/WarriorPlayCheckpointTypes.h"

/**
 * 플레이 체크포인트(M22 최종) 꺼내기·넣기. 스테이지 레벨의 첫 번째 플레이어 기준.
 *
 * - Capture: 인벤토리(골드·아이템), 판 강화, GAS 현재값, 판 통계를 한 묶음으로 꺼낸다.
 * - Restore: 강화 → GAS 현재값 → 인벤토리 → 판 통계 순서로 넣는다.
 *   · 강화를 먼저 걸어야 최대치(MaxHealth 등)가 맞춰진 뒤 현재값을 그 안으로 넣을 수 있다.
 *   · 판 통계를 마지막에 넣어야 인벤토리 골드 변화가 골드 사용량으로 세지지 않는다.
 * - 웨이브 위치(다음 웨이브의 쉬는 시간부터)는 여기서 다루지 않는다(GameMode, 2단계).
 */
namespace WarriorPlayCheckpoint
{
	//지금 판 안 값을 꺼낸다. 스테이지 기록 중이 아니거나 플레이어가 없으면 false, OutError에 이유
	PROJECTWARRIOR_API bool Capture(const UObject* WorldContextObject, FWarriorPlayCheckpoint& OutCheckpoint, FString& OutError);

	//체크포인트 값을 넣는다. 버전이 다르거나 플레이어가 없으면 false, OutError에 이유. 찾지 못한 아이템·강화 에셋은 건너뛰고 경고만 남긴다
	PROJECTWARRIOR_API bool Restore(const UObject* WorldContextObject, const FWarriorPlayCheckpoint& InCheckpoint, FString& OutError);

	//서버로 보낼 JSON 본문(칸 이름 첫 글자 소문자)
	PROJECTWARRIOR_API bool ToJson(const FWarriorPlayCheckpoint& InCheckpoint, FString& OutJson);

	PROJECTWARRIOR_API bool FromJson(const FString& InJson, FWarriorPlayCheckpoint& OutCheckpoint);

	//로그용 한 줄 요약
	PROJECTWARRIOR_API FString Describe(const FWarriorPlayCheckpoint& InCheckpoint);
}
