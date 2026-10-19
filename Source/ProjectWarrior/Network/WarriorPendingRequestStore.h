// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Network/WarriorApiTypes.h"
#include "WarriorPendingRequestStore.generated.h"

/**
 * 보관한 요청 하나. Saved/PendingRequests/<계정 폴더>/<requestId>.json에 이 모양 그대로 저장한다.
 * 토큰·비밀번호는 넣지 않는다(본문은 서버에 보내는 JSON 그대로, 결과 제출에는 비밀값이 없다).
 */
USTRUCT()
struct PROJECTWARRIOR_API FWarriorPendingRequest
{
	GENERATED_BODY()

	//파일 모양 버전. 칸을 바꾸면 올린다
	UPROPERTY()
	int32 SchemaVersion = 1;

	//요청 종류(파일에는 이름으로 저장)
	UPROPERTY()
	EWarriorPendingKind Kind = EWarriorPendingKind::None;

	UPROPERTY()
	FString Verb;

	UPROPERTY()
	FString Path;

	UPROPERTY()
	FString Body;

	//본문의 requestId. 파일 이름이자 재전송 키
	UPROPERTY()
	FString RequestId;

	//종류 안에서 찾는 키(결과 제출이면 stagePlayId)
	UPROPERTY()
	FString StoreKey;

	UPROPERTY()
	FDateTime CreatedAt;

	//이 시각이 지나면 보내지 않고 지운다(결과 제출: 플레이 마감)
	UPROPERTY()
	FDateTime ExpiresAt;

	//깨짐 확인용 해시(SHA-1). 위변조 방지가 아니다(서버가 다시 검사한다)
	UPROPERTY()
	FString Hash;
};

/**
 * 보관·재전송 부품(S5 UF-3). 계속 실패한 상태 변경 요청을 계정별 파일로 보관한다.
 * - 계정 폴더 이름은 계정 ID의 해시라 경로에 계정 ID가 그대로 남지 않는다.
 * - 깨진 파일(JSON을 못 읽음·해시가 다름)은 보내지 않고 .bad로 이름을 바꿔 둔다.
 * - 그 계정이 로그인했을 때만 읽는다(UWarriorApiSubsystem이 로그인 뒤 재전송).
 * 상태가 없는 함수 모음이라 서브시스템이 아니라 정적 함수로 둔다.
 */
struct PROJECTWARRIOR_API FWarriorPendingRequestStore
{
	//보관한다(같은 requestId면 덮어쓴다). 해시는 여기서 채운다
	static bool Save(const FString& InAccountId, FWarriorPendingRequest InEntry);

	//그 계정의 보관 요청을 모두 읽는다(만든 순). 깨진 파일은 .bad로 옮기고 OutCorruptedCount에 센다
	static TArray<FWarriorPendingRequest> LoadAll(const FString& InAccountId, int32& OutCorruptedCount);

	//보관본 하나를 지운다
	static bool Remove(const FString& InAccountId, const FString& InRequestId);

	//종류와 키로 하나 찾는다(예: 이 플레이의 보관 결과가 있는가)
	static bool FindByKey(const FString& InAccountId, EWarriorPendingKind InKind, const FString& InStoreKey, FWarriorPendingRequest& OutEntry);

	//그 계정의 보관 폴더(Saved/PendingRequests/<해시 16자>)
	static FString GetAccountDir(const FString& InAccountId);

	//칸으로 계산한 해시(파일에 적힌 Hash와 비교한다)
	static FString ComputeHash(const FWarriorPendingRequest& InEntry);
};
