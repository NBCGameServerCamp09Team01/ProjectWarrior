// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Network/WarriorGuildTypes.h"
#include "ProjectWarrior/Network/WarriorRealtimeSubsystem.h"
#include "WarriorGuildSubsystem.generated.h"

class FJsonObject;
struct FWarriorApiResult;

//내 길드(G4)를 새로 받았다. 길드 팝업은 이때 화면 전체를 다시 그린다
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorMyGuildUpdated);

//내 길드 ID가 바뀌었다(가입·탈퇴·해체·로그인·로그아웃). 미소속은 빈 값. 채팅의 길드 탭이 쓴다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorMyGuildChanged, const FString&, OldGuildId, const FString&, NewGuildId);

//길드 목록(G1)·받은 신청(G7)을 새로 받았다
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorGuildListUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWarriorGuildJoinRequestsUpdated);

//쓰기 동작이 끝났다. 실패면 ErrorCode(예: GUILD_FULL)와 화면 문구(오류 문구 표)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWarriorGuildActionCompleted, EWarriorGuildAction, Action, bool, bSuccess, const FString&, ErrorCode, const FText&, Message);

//조회(G1·G4·G7)가 실패했다. 화면은 "불러오지 못함" 상태와 다시 불러오기를 보인다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorGuildRequestFailed, const FString&, ErrorCode, const FText&, Message);

/**
 * 길드(M23 소셜)를 서버에서 받아 들고 있다. 명세: 루트 docs/contracts/guild-api.md v1, realtime-api.md v1.4 GUILD_*
 *
 * - 화면은 G4(내 길드) 하나로 다시 그린다(명세 "엔드포인트" 아래 규칙). 쓰기 응답은 "성공했다"는 확인으로만 쓰고, 성공하면 G4를 다시 받는다.
 *   G3 만들기만 응답이 G4 모양이라 바로 적용한다.
 * - G4를 받는 때: 로그인 성공 직후(채팅이 팝업과 상관없이 GetMyGuildId를 씀), 실시간 GUILD_*를 받았을 때(0.5초 모아 한 번),
 *   실시간이 다시 연결됐을 때(끊긴 동안 놓친 알림 보충), 쓰기 성공 뒤. 로그인 상태가 끝나면 받지 않고 모두 비운다.
 * - 요청은 모두 UWarriorApiSubsystem::Send. 디스크에 보관하지 않고(StoreKind None) "다시 시도 / 종료"를 묻지 않는다(bAskOnFail false).
 *   retryable 실패(연결 실패·VERSION_CONFLICT·5xx)의 자동 재시도는 공통 처리가 같은 본문(같은 requestId)으로 한다.
 * - 쓰기는 한 번에 하나(IsActionInFlight). 새로 누른 동작은 새 requestId.
 * - 오류를 화면에 보이는 것은 위젯(U3)이 한다. 여기서는 코드와 문구(FWarriorErrorText, 맥락 Guild)만 넘긴다.
 * - 늦게 온 지난 응답은 요청 번호로, 다른 세션의 응답은 세션 번호로 버린다.
 *
 * 연동 지점(다른 모듈):
 * - 채팅: GetMyGuildId·OnMyGuildChanged·HasLoadedMyGuild. 로그인 직후 G4 응답 전에는 HasLoadedMyGuild가 false다(그동안 "길드 없음"으로 보임).
 * - 실시간: UWarriorRealtimeSubsystem::OnMessage(U1)에서 GUILD_*만 본다. PRESENCE_CHANGED는 Lv2.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorGuildSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorGuildSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin 읽기
	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	const FWarriorMyGuild& GetMyGuild() const { return MyGuild; }

	//내 길드 ID(숫자 문자열). 미소속이거나 아직 받지 않았으면 빈 값
	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	FString GetMyGuildId() const { return MyGuild.bHasGuild ? MyGuild.Guild.GuildId : FString(); }

	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	bool IsInGuild() const { return MyGuild.bHasGuild; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	bool IsLeader() const { return MyGuild.MyRole == EWarriorGuildRole::Leader; }

	//이번 로그인에서 G4를 한 번이라도 받았나
	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	bool HasLoadedMyGuild() const { return bMyGuildLoaded; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	const TArray<FWarriorGuildSummary>& GetGuildList() const { return GuildList; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	bool HasMoreGuilds() const { return !GuildListCursor.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	const TArray<FWarriorGuildJoinRequest>& GetReceivedJoinRequests() const { return ReceivedJoinRequests; }

	//쓰기 요청이 응답을 기다리는 중인가. 화면은 이 동안 버튼을 막는다
	UFUNCTION(BlueprintPure, Category = "Warrior|Guild")
	bool IsActionInFlight() const { return bActionInFlight; }
	//~ End 읽기

	//~ Begin 조회
	//G4 내 길드. 결과는 OnMyGuildUpdated(실패면 OnRequestFailed). 받는 중이면 끝난 뒤 한 번 더 받는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void RefreshMyGuild();

	//G1 길드 목록 첫 묶음. InNamePrefix가 비어 있으면 전부(최근 만든 순), 있으면 이름 앞부분 검색(대소문자 가림)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void RequestGuildList(const FString& InNamePrefix);

	//G1 다음 묶음(서버가 준 커서 그대로). 더 없거나 받는 중이면 아무것도 하지 않는다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void RequestMoreGuilds();

	//G7 받은 신청(길드장만)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	void RequestReceivedJoinRequests();
	//~ End 조회

	//~ Begin 쓰기. 시작했으면 true(결과는 OnGuildActionCompleted). 이미 쓰는 중이거나 로그인하지 않았으면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool CreateGuild(const FString& InName);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool ApplyToGuild(const FString& InGuildId);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool CancelJoinRequest(const FString& InJoinRequestId);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool ApproveJoinRequest(const FString& InJoinRequestId);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool RejectJoinRequest(const FString& InJoinRequestId);

	//탈퇴. 길드장이 혼자면 서버가 해체한다(응답 disbanded)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool LeaveGuild();

	UFUNCTION(BlueprintCallable, Category = "Warrior|Guild")
	bool DisbandGuild();
	//~ End 쓰기

	//~ Begin 응답 읽기(자동 시험에서도 부른다). 성공 응답 { data, meta } 전체를 받는다. 못 읽으면 false
	static bool ParseMyGuild(const FString& InBody, FWarriorMyGuild& OutMyGuild);
	static bool ParseGuildList(const FString& InBody, TArray<FWarriorGuildSummary>& OutGuilds, FString& OutNextCursor);
	static bool ParseJoinRequests(const FString& InBody, TArray<FWarriorGuildJoinRequest>& OutJoinRequests);
	//~ End 응답 읽기

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorMyGuildUpdated OnMyGuildUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorMyGuildChanged OnMyGuildChanged;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorGuildListUpdated OnGuildListUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorGuildJoinRequestsUpdated OnJoinRequestsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorGuildActionCompleted OnGuildActionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Guild")
	FOnWarriorGuildRequestFailed OnRequestFailed;

protected:
	//실시간 GUILD_* 알림을 모으는 시간(초). 알림이 몰려도 G4는 이 간격에 한 번(명세: 0.5초 모아 다시 받기)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild")
	float RealtimeRefreshDelaySeconds = 0.5f;

	//G1 한 묶음 수(명세 size 1~50)
	UPROPERTY(EditDefaultsOnly, Category = "Warrior|Guild")
	int32 GuildListPageSize = 20;

private:
	//요청 하나. 공통 설정(StoreKind None, bAskOnFail false, 맥락 Guild)을 채워 보낸다. 시작하지 못하면 false
	bool Send(const FString& InVerb, const FString& InPath, const FString& InBody, const FString& InRequestId,
		TFunction<void(const FWarriorApiResult&)>&& OnDone);

	//쓰기 하나를 보낸다. 본문은 { requestId }, InExtraField가 있으면 그 칸 하나를 더한다(G3 name, G5 guildId)
	bool SendAction(EWarriorGuildAction InAction, const FString& InVerb, const FString& InPath, const FString& InExtraField = FString(), const FString& InExtraValue = FString());
	void HandleActionResponse(EWarriorGuildAction InAction, int32 InSessionSerial, const FWarriorApiResult& InResult);
	void FinishAction(EWarriorGuildAction InAction, bool bInSuccess, const FString& InCode, const FText& InMessage);

	void HandleMyGuildResponse(int32 InSerial, int32 InSessionSerial, const FWarriorApiResult& InResult);
	void HandleGuildListResponse(int32 InSerial, int32 InSessionSerial, bool bInAppend, const FWarriorApiResult& InResult);
	void HandleJoinRequestsResponse(int32 InSerial, int32 InSessionSerial, const FWarriorApiResult& InResult);

	//새 내 길드를 적용한다. 길드 ID가 바뀌었으면 OnMyGuildChanged도 낸다
	void ApplyMyGuild(const FWarriorMyGuild& InMyGuild);

	//보낼 때와 같은 세션인가(로그아웃·다시 로그인 뒤의 응답은 버린다)
	bool IsSameSession(int32 InSessionSerial) const;
	int32 GetSessionSerial() const;

	//조회 실패를 알린다. 401은 Auth가 이미 로그인 상태를 끝냈으므로 알리지 않는다
	void NotifyRequestFailed(const FWarriorApiResult& InResult);

	//실시간 GUILD_* 알림 → 모아서 G4(필요하면 G7)
	void HandleRealtimeMessage(const FString& InType, const FString& InId, const TSharedPtr<FJsonObject>& InData);
	void ScheduleRealtimeRefresh(bool bInJoinRequests);
	bool HandleRealtimeRefreshTick(float InDeltaTime);

	UFUNCTION()
	void HandleRealtimeStateChanged(EWarriorRealtimeState InState);

	UFUNCTION()
	void HandleLoginCompleted(bool bSuccess, const FString& ErrorCode, const FText& Message);

	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason InReason, const FText& InMessage);

	//모두 비운다(로그인 상태가 끝났을 때)
	void ResetState();

	UPROPERTY(Transient)
	FWarriorMyGuild MyGuild;

	UPROPERTY(Transient)
	TArray<FWarriorGuildSummary> GuildList;

	UPROPERTY(Transient)
	TArray<FWarriorGuildJoinRequest> ReceivedJoinRequests;

	bool bMyGuildLoaded = false;

	//G4: 받는 중인가, 받는 동안 또 받아야 할 일이 생겼나
	bool bMyGuildInFlight = false;
	bool bMyGuildDirty = false;
	int32 MyGuildSerial = 0;

	//G1: 지금 검색어와 다음 커서
	FString GuildListNamePrefix;
	FString GuildListCursor;
	bool bGuildListInFlight = false;
	int32 GuildListSerial = 0;

	//G7: 이번 로그인에서 받은 적이 있나(받은 적이 있을 때만 알림으로 다시 받는다)
	bool bJoinRequestsRequested = false;
	int32 JoinRequestsSerial = 0;

	bool bActionInFlight = false;

	//실시간 알림 모으기
	FTSTicker::FDelegateHandle RealtimeRefreshTicker;
	bool bRealtimeRefreshJoinRequests = false;

	//실시간이 처음 연결된 뒤인가(처음 연결은 로그인 직후 G4와 겹치므로 다시 받지 않는다)
	bool bRealtimeWasConnected = false;

	FDelegateHandle RealtimeMessageHandle;
};
