// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 응답의 계정 스냅샷(account)을 사본에 적용해도 되는지 판정한다(M25, 루트 docs/contracts/account-api.md "받는 쪽 규칙"·"버전 규칙").
 * 순수 함수라 자동화 시험으로 확인한다(Network/Tests/WarriorAccountSyncTests.cpp).
 */
namespace WarriorAccountSync
{
	//요청을 보낸 뒤 로그아웃·다시 로그인이 있었으면 그 응답의 계정 값은 쓰지 않는다(결과 제출·보관 결과 재전송, 재전송 길과 같은 조건)
	inline bool IsSameSession(bool bLoggedIn, int32 CurrentSessionSerial, int32 SentSessionSerial,
		const FString& SentAccountId, const FString& SnapshotAccountId)
	{
		return bLoggedIn && CurrentSessionSerial == SentSessionSerial
			&& !SentAccountId.IsEmpty() && SnapshotAccountId == SentAccountId;
	}

	//마지막으로 적용한 것보다 작은 version은 늦게 온 응답이라 버린다. 같으면 적용한다
	inline bool IsNotOlder(int64 InVersion, int64 LastAppliedVersion)
	{
		return InVersion >= LastAppliedVersion;
	}
}
