// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectWarrior/Auth/WarriorAuthTypes.h"
#include "ProjectWarrior/Chat/WarriorChatTypes.h"
#include "ProjectWarrior/Network/WarriorRealtimeSubsystem.h"
#include "WarriorChatSubsystem.generated.h"

class FJsonObject;
struct FWarriorApiResult;

//한 탭의 줄이 바뀌었다(새 줄·내 줄 상태·내역·비움). 채팅 창은 GetLines로 다시 그린다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorChatUpdated, EWarriorChatScope, Scope);

//보내기가 막혔다/풀렸다(429). 막혔으면 Notice = 화면 안 안내, 남은 초는 GetSendBlockedSeconds
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorChatSendStateChanged, bool, bBlocked, const FText&, Notice);

//보낸 글을 서버가 받지 않았다(400 글 규칙·403 길드 없음·429). 줄은 지웠으니 채팅 창은 Text를 입력 칸에 되돌리고 Message를 보인다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWarriorChatSendRejected, EWarriorChatScope, Scope, const FString&, Code, const FText&, Message, const FString&, Text);

//내 길드 상태가 바뀌었다(길드 탭 안내 "길드에 가입하면 …")
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorChatGuildStateChanged, EWarriorChatGuildState, State);

//탭의 "안 읽음"이 바뀌었다(U-C4). 채팅 창 탭 옆 점
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWarriorChatUnreadChanged, EWarriorChatScope, Scope, bool, bUnread);

/**
 * 채팅(M23). 명세: 루트 docs/contracts/chat-api.md(CH1·CH2·CH3, "게임 처리 규칙"), realtime-api.md CHAT_MESSAGE
 *
 * - GameInstance 서브시스템이라 스테이지를 다녀와도 받은 줄이 남는다. 탭마다 MaxLinesPerTab(50)줄, 넘으면 옛것부터 버린다.
 * - 보내기: 글 규칙을 먼저 보고(최종은 서버) 내 줄을 "보내는 중"으로 먼저 붙인 뒤 CH1·CH2. 응답 data로 그 줄을 바꾼다.
 *   보관하지 않고(StoreKind None) 묻지 않는다(bAskOnFail false). 끝내 실패하면 "보내지 못함" 줄 → RetryLine = 같은 requestId·같은 글.
 *   429는 자동으로 다시 보내지 않고 retryAfterSeconds 동안 보내기를 막는다. 400·403은 줄을 지우고 OnSendRejected.
 * - 받기: 실시간 OnMessage(U1)의 CHAT_MESSAGE만. 같은 messageId는 한 번(내 글은 응답과 실시간이 둘 다 옴), 내 길드가 아닌 길드 줄은 버린다.
 * - 길드 내역(CH3): 실시간 연결이 붙을 때(로그인 뒤·다시 붙음)와 길드가 바뀔 때 첫 묶음, 위로 끝까지 가면 LoadGuildHistory(true).
 * - 내 길드: U2(UWarriorGuildSubsystem::OnMyGuildChanged)가 SetMyGuildId를 부르기 전에는 서버 응답(CH2·CH3의 guildId, 403)으로 안다.
 * - 안 읽음(U-C4): 남이 쓴 줄이 창이 닫혔거나 다른 탭을 볼 때 오면 그 탭에 점. 창이 SetViewedTab으로 지금 보는 탭을 알린다.
 *   다시 붙어 채운 길드 내역에 새 줄이 있어도 점(로그인 뒤 첫 묶음은 빼고 — 3일 내역마다 점이 뜨지 않게).
 * - 로그인 상태가 끝나면 모두 비운다. 로그에 채팅 글은 남기지 않는다.
 */
UCLASS(Config = Game)
class PROJECTWARRIOR_API UWarriorChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorChatSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//글을 보낸다. Sending이면 보냈고(결과는 OnChatUpdated·OnSendRejected), 아니면 보내지 않은 까닭
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	EWarriorChatSendResult SendMessage(EWarriorChatScope Scope, const FString& Text);

	//"보내지 못함" 줄을 같은 requestId·같은 글로 다시 보낸다. 그런 줄이 없거나 막혀 있으면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	bool RetryLine(EWarriorChatScope Scope, const FString& RequestId);

	//"보내지 못함" 줄을 지운다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	void DiscardLine(EWarriorChatScope Scope, const FString& RequestId);

	//길드 내역(CH3). bOlder면 지금 가진 가장 옛 줄보다 이전 묶음, 아니면 최신 묶음을 합친다. 보내지 못했거나(로그인 전·받는 중·더 없음) 시작 못 하면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	bool LoadGuildHistory(bool bOlder);

	//한 탭의 줄(옛것 → 새것)
	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	TArray<FWarriorChatLine> GetLines(EWarriorChatScope Scope) const;

	//위로 더 받을 길드 내역이 있나(커서가 있고 줄 수가 상한 아래)
	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	bool HasOlderGuildHistory() const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	bool IsLoadingGuildHistory() const { return bLoadingGuildHistory; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	EWarriorChatGuildState GetGuildState() const { return GuildState; }

	//아는 내 길드 ID. 모르면 빈 값
	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	FString GetMyGuildId() const { return MyGuildId; }

	//보내기가 막힌 남은 초(올림). 막히지 않았으면 0. 채팅 창이 버튼 글자에 쓴다
	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	int32 GetSendBlockedSeconds() const;

	//입력 칸 최대 글자(코드 포인트)
	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	int32 GetMaxTextLength() const { return MaxTextLength; }

	//채팅 창이 지금 보는 탭(bInOpen = 창이 떠 있음). 보는 탭은 안 읽음을 지운다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Chat")
	void SetViewedTab(EWarriorChatScope InScope, bool bInOpen);

	UFUNCTION(BlueprintPure, Category = "Warrior|Chat")
	bool HasUnread(EWarriorChatScope Scope) const { return Scope == EWarriorChatScope::Guild ? bUnreadGuild : bUnreadGlobal; }

	//내 길드가 바뀌었다(빈 값 = 길드 없음). 길드 탭을 비우고 길드가 있으면 첫 묶음을 받는다. U2 길드 서브시스템이 부른다
	void SetMyGuildId(const FString& InGuildId);

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Chat")
	FOnWarriorChatUpdated OnChatUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Chat")
	FOnWarriorChatSendStateChanged OnSendStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Chat")
	FOnWarriorChatSendRejected OnSendRejected;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Chat")
	FOnWarriorChatGuildStateChanged OnGuildStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Chat")
	FOnWarriorChatUnreadChanged OnUnreadChanged;

protected:
	//탭마다 남기는 줄 수(C-6)
	UPROPERTY(Config)
	int32 MaxLinesPerTab = 50;

	//글 최대 길이(코드 포인트, 서버 pw01.chat.max-length와 같게)
	UPROPERTY(Config)
	int32 MaxTextLength = 100;

	//길드 내역 한 묶음(서버 기본 10, 최대 50)
	UPROPERTY(Config)
	int32 HistoryPageSize = 10;

private:
	//~ Begin 실시간(U1)
	void HandleRealtimeMessage(const FString& InType, const FString& InId, const TSharedPtr<FJsonObject>& InData);

	UFUNCTION()
	void HandleRealtimeStateChanged(EWarriorRealtimeState InState);
	//~ End 실시간

	UFUNCTION()
	void HandleSessionEnded(EWarriorSessionEndReason Reason, const FText& Message);

	//CH1·CH2 요청을 보낸다. 시작하지 못하면 false
	bool SendLine(EWarriorChatScope InScope, const FString& InRequestId, const FString& InText);
	void HandleSendResult(EWarriorChatScope InScope, const FString& InRequestId, int32 InSessionSerial, const FWarriorApiResult& InResult);
	void HandleHistoryResult(bool bOlder, int32 InSessionSerial, int32 InGuildTabSerial, const FWarriorApiResult& InResult);

	//403 GUILD_NOT_MEMBER: 길드 없음으로 두고 길드 탭을 비운다
	void ApplyGuildNotMember();

	//서버가 알려 준 내 길드 ID(CH2 응답·CH3·실시간). 모르던 때만 받아들이고, 다르면 길드가 바뀐 것으로 본다
	void AdoptGuildId(const FString& InGuildId);

	void SetGuildState(EWarriorChatGuildState InState);
	void ResetGuildTab();

	//429: InSeconds 동안 보내기를 막는다
	void BlockSending(const FString& InCode, int32 InSeconds);
	bool HandleUnblockTick(float InDeltaTime);
	void StopUnblockTicker();

	void ClearAll();

	void SetUnread(EWarriorChatScope InScope, bool bInUnread);

	//이 요청의 내 줄 번호. 없으면 INDEX_NONE
	static int32 FindPendingLine(const TArray<FWarriorChatLine>& InLines, const FString& InRequestId);

	TArray<FWarriorChatLine>& LinesOf(EWarriorChatScope InScope) { return InScope == EWarriorChatScope::Guild ? GuildLines : GlobalLines; }
	const TArray<FWarriorChatLine>& LinesOf(EWarriorChatScope InScope) const { return InScope == EWarriorChatScope::Guild ? GuildLines : GlobalLines; }

	int32 CurrentSessionSerial() const;

	TArray<FWarriorChatLine> GlobalLines;
	TArray<FWarriorChatLine> GuildLines;

	FString MyGuildId;
	EWarriorChatGuildState GuildState = EWarriorChatGuildState::Unknown;

	//다음 "위로 더 받기"의 before. 빈 값이면 더 없음
	FString GuildHistoryCursor;
	bool bLoadingGuildHistory = false;

	//안 읽음(U-C4)과 채팅 창이 보는 탭
	bool bUnreadGlobal = false;
	bool bUnreadGuild = false;
	bool bPanelOpen = false;
	EWarriorChatScope ViewedScope = EWarriorChatScope::Global;

	//길드 탭을 비울 때마다 1 오른다. 비우기 전에 보낸 내역 요청의 응답은 버린다(옛 길드 줄이 섞이지 않게)
	int32 GuildTabSerial = 0;

	//보내기 막힘(429). FPlatformTime::Seconds 기준
	double SendBlockedUntil = 0.0;
	FTSTicker::FDelegateHandle UnblockTickerHandle;

	FDelegateHandle RealtimeMessageHandle;
};
