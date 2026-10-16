// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WarriorGuildTypes.generated.h"

/**
 * 길드 화면이 쓰는 값. 명세: 루트 docs/contracts/guild-api.md v1 (G1~G12)
 * ID는 서버와 같이 숫자 문자열(FString)로 들고 있다(길드 ID "7"). 시각은 UTC.
 * 서버 응답의 null(미소속의 guild, 마지막 묶음의 nextCursor)은 bHas…·빈 값으로 나타낸다.
 */

//길드 안 역할(응답 myRole·members[].role LEADER·MEMBER). 미소속은 None
UENUM(BlueprintType)
enum class EWarriorGuildRole : uint8
{
	None,
	Member,
	Leader
};

//쓰기 동작(OnGuildActionCompleted로 어느 동작이 끝났는지 알린다)
UENUM(BlueprintType)
enum class EWarriorGuildAction : uint8
{
	Create,		// G3 만들기
	Apply,		// G5 가입 신청
	Cancel,		// G6 신청 취소
	Approve,	// G8 신청 수락
	Reject,		// G9 신청 거절
	Leave,		// G11 탈퇴(길드장이 혼자면 해체)
	Disband		// G12 해체
};

//길드 요약(G1 목록 줄, G4 guild). 명세 "공통 모양 GuildSummary"
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorGuildSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString GuildId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString LeaderAccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString LeaderNickname;

	//지금 길드원 수(길드장 포함)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	int32 MemberCount = 0;

	//정원. MemberCount == MaxMembers면 신청 버튼을 끈다(서버도 GUILD_FULL로 거절)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	int32 MaxMembers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FDateTime CreatedAt;
};

//길드원 한 명(G4 members[], G8 응답 member)
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorGuildMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString AccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	EWarriorGuildRole Role = EWarriorGuildRole::Member;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FDateTime JoinedAt;

	//지금 실시간 연결이 있나(서버 어느 대든). 끊긴 뒤 최대 150초까지 true로 남을 수 있다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	bool bOnline = false;
};

//가입 신청 하나(G4 myJoinRequests[], G7 joinRequests[]). 명세 "공통 모양 JoinRequest"
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorGuildJoinRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString JoinRequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString GuildId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString GuildName;

	//신청한 사람
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString AccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FDateTime RequestedAt;

	//유효 기간 끝(신청 + 3일)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FDateTime ExpiresAt;
};

//만들기 조건 하나(G4 create.requirements[]). 지금은 Type STAGE_CLEARED 하나
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorGuildRequirement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FString Type;

	//클리어해야 하는 스테이지(예: stage.01.01)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FName StageId;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	bool bMet = false;
};

//내 길드 화면 전체(G4 GET /accounts/me/guild, G3 응답). 길드 팝업은 이 값 하나로 다시 그린다
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorMyGuild
{
	GENERATED_BODY()

	//소속 길드가 있나(응답 guild가 null이 아님)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	bool bHasGuild = false;

	//소속 길드. bHasGuild가 false면 빈 값
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	FWarriorGuildSummary Guild;

	//내 역할. 미소속이면 None
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	EWarriorGuildRole MyRole = EWarriorGuildRole::None;

	//길드장 먼저, 그다음 들어온 순. 미소속이면 비어 있다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	TArray<FWarriorGuildMember> Members;

	//받은 대기 중 신청 수. 길드장이 아니면 0
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	int32 PendingJoinRequestCount = 0;

	//내 대기 중 신청(유효 기간 안, 오래된 순). 길드원이면 비어 있다
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	TArray<FWarriorGuildJoinRequest> MyJoinRequests;

	//지금 길드를 만들 수 있나(미소속 + 조건 충족). 서버 길드 수 상한은 들어 있지 않다(만들 때 판정)
	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	bool bCanCreate = false;

	UPROPERTY(BlueprintReadOnly, Category = "Warrior|Guild")
	TArray<FWarriorGuildRequirement> CreateRequirements;
};
