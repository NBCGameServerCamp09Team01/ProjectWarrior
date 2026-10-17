// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 적 난이도 배율(M9)을 언제 곱할지 정하는 판정. AWarriorAICharacter가 쓴다.
 * 배율(웨이브 생성기·졸개 소환이 스폰 직후 넘김)과 시작 데이터(비동기 로드 콜백에서 시작 GE 적용)는 어느 쪽이 먼저 올지 정해져 있지 않다.
 * 둘 다 준비됐을 때 한 번만 곱하게 한다. 배율이 1이면 곱하지 않고 "곱함" 표시도 세우지 않는다(나중에 온 배율을 막지 않게).
 * 판정만 하고 값은 바꾸지 않으므로 자동 시험으로 두 순서를 모두 확인한다(Stage/Tests/WarriorDifficultyScaleTests.cpp).
 */
struct FWarriorDifficultyScaleGate
{
	float HealthMultiplier = 1.f;
	float DamageMultiplier = 1.f;

	// 시작 데이터(GE) 적용이 끝났는가. 그 전에 받은 배율은 곱하기를 미룬다
	bool bStartUpDataApplied = false;

	// 1이 아닌 배율을 이미 곱했는가. 두 번 곱하지 않는다
	bool bApplied = false;

	// 배율을 받는다. 0 이하는 1로 둔다(체력 0인 적이 생기지 않게). 지금 곱해야 하면 true
	bool SetScale(float InHealthMultiplier, float InDamageMultiplier)
	{
		HealthMultiplier = InHealthMultiplier > 0.f ? InHealthMultiplier : 1.f;
		DamageMultiplier = InDamageMultiplier > 0.f ? InDamageMultiplier : 1.f;
		return TryApply();
	}

	// 시작 데이터 적용이 끝났다. 지금 곱해야 하면 true
	bool MarkStartUpDataApplied()
	{
		bStartUpDataApplied = true;
		return TryApply();
	}

private:
	bool TryApply()
	{
		if (!bStartUpDataApplied || bApplied)
		{
			return false;
		}
		//1이면 곱할 것이 없다. 표시를 세우지 않아야 나중에 온 배율(예: 시작 데이터가 없는 적은 빙의 때 먼저 여기 옴)을 곱할 수 있다
		if (FMath::IsNearlyEqual(HealthMultiplier, 1.f) && FMath::IsNearlyEqual(DamageMultiplier, 1.f))
		{
			return false;
		}
		bApplied = true;
		return true;
	}
};
