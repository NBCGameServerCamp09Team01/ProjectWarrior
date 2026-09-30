// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AttributeSet.h"
#include "WarriorAccountTypes.h"
#include "ProjectWarrior/Stats/WarriorStatTypes.h"
#include "WarriorAccountSubsystem.generated.h"

class UWarriorProfileStatsSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorAccountChanged, const FWarriorAccountData&, AccountData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarriorStageRewarded, const FWarriorStageReward&, Reward);

/** 스탯 하나가 GAS 어트리뷰트에 주는 값. 캐릭터에 성장을 적용하는 쪽이 읽는다 */
struct FWarriorAccountStatBonus
{
	FGameplayTag StatTag;
	FGameplayAttribute Attribute;
	//투자한 포인트 × 포인트당 증가량
	float Bonus = 0.f;
};

/**
 * 계정 서브시스템. 계정 레벨, 경험치, 스탯 포인트, 스탯 투자, 스킬 해금을 관리한다.
 *
 * 흐름
 * - 스테이지가 끝나면 통계 서브시스템이 기록을 확정하고 OnStageRecorded를 방송한다.
 *   이 서브시스템이 그것을 받아 보상(경험치 → 레벨 → 스탯 포인트)을 계산한다.
 * - 메인메뉴의 성장 화면이 InvestStatPoint·UnlockSkill을 호출한다.
 * - 스테이지 시작 때 캐릭터가 BuildStatBonuses로 투자 내역을 읽어 GAS에 적용한다(적용은 성장 담당).
 *
 * 서버 연동 전략(2주차)
 * - 서버가 계정 값의 최종 기준이다. 지금의 로컬 계산은 서버가 없는 1주차용 임시 규칙이다.
 * - 값이 바뀔 때마다 변경 내역(FWarriorAccountLedgerEntry)을 남긴다. 서버 연동 뒤에는 이 내역을 서버에 보내고,
 *   서버가 돌려준 계정 데이터를 ApplyServerSnapshot으로 덮어쓴다.
 * - 외부 재화는 서버만 추적한다. 클라이언트는 서버가 내려 준 잔액을 보여 주기만 한다.
 *
 * 저장: 지금은 메모리에만 있다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorAccountSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//임시 규칙. 기획 확정 또는 서버 연동 시 데이터(DataAsset·서버 응답)로 교체한다.
	struct FRules
	{
		int32 MaxLevel = 99;
		int32 ClearedExp = 100;		//클리어 보상
		int32 FailedExp = 20;		//실패 보상(아주 적게)
		int32 StatPointsPerLevel = 2;
		//레벨 L에서 다음 레벨까지 필요한 경험치 = ExpBase + (L - 1) * ExpStep
		int32 ExpBase = 100;
		int32 ExpStep = 50;
		//보상을 준 기록 번호를 기억하는 최대 개수
		int32 MaxRewardedRecords = 100;
		//보관하는 변경 내역 최대 개수(넘으면 오래된 것부터 제거)
		int32 MaxLedgerEntries = 200;
	};

	//스탯 하나의 정의: 어떤 어트리뷰트에 얼마씩, 최대 몇 포인트까지
	struct FStatDefinition
	{
		FGameplayTag Tag;
		FGameplayAttribute Attribute;
		float BonusPerPoint = 0.f;
		int32 MaxPoints = 0;
	};

	static UWarriorAccountSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ 조회 (메인메뉴·성장 화면·결과 화면·캐릭터)
	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	const FWarriorAccountData& GetAccountData() const { return Data; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	int32 GetAccountLevel() const { return Data.AccountLevel; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	int32 GetStatPoints() const { return Data.StatPoints; }

	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	int32 GetExperience() const { return Data.Experience; }

	//현재 레벨에서 다음 레벨까지 필요한 경험치. 최대 레벨이면 0
	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	int32 GetExperienceToNextLevel() const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	int32 GetInvestedPoints(FGameplayTag StatTag) const;

	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	bool IsSkillUnlocked(FGameplayTag SkillTag) const { return Data.UnlockedSkills.HasTagExact(SkillTag); }

	//스탯이 지금 올려 주는 값(투자한 포인트 × 포인트당 증가량). 알 수 없는 태그면 0
	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	float GetStatBonus(FGameplayTag StatTag) const;

	//스킬 해금에 필요한 스탯 포인트. 알 수 없는 스킬이면 -1
	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	int32 GetSkillCost(FGameplayTag SkillTag) const;

	//투자할 수 있는 스탯의 태그 목록(성장 화면이 버튼을 만들 때 사용)
	UFUNCTION(BlueprintPure, Category = "Warrior|Account")
	TArray<FGameplayTag> GetInvestableStats() const;

	//투자한 스탯 전체를 어트리뷰트별 증가량으로 돌려준다. 스테이지 시작 때 캐릭터에 적용하는 쪽이 사용한다
	TArray<FWarriorAccountStatBonus> BuildStatBonuses() const;

	//마지막으로 받은 스테이지 보상. 아직 없으면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Account")
	bool GetLastStageReward(FWarriorStageReward& OutReward) const;

	//~ 변경 (성장 화면)
	//스탯 포인트를 투자한다. 포인트가 모자라거나 최대치를 넘으면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Account")
	bool InvestStatPoint(FGameplayTag StatTag, int32 Points = 1);

	//스킬을 해금한다. 이미 해금했거나 포인트가 모자라면 false
	UFUNCTION(BlueprintCallable, Category = "Warrior|Account")
	bool UnlockSkill(FGameplayTag SkillTag);

	//~ 서버 연동 준비 (지금은 임시 구현)
	//서버가 내려 준 계정 데이터로 통째로 덮어쓴다. 서버가 최종 기준이므로 로컬 값보다 항상 우선한다.
	//TODO(server): 서버 응답을 받는 웹 통신 서브시스템이 호출한다. 지금은 호출하는 곳이 없다.
	void ApplyServerSnapshot(const FWarriorAccountData& InServerData);

	//서버에 아직 확인받지 못한 변경 내역. 서버 전송 대상
	//TODO(server): 서버에 보낼 때 EntryId를 요청 번호로 쓴다. 같은 번호가 다시 오면 서버가 무시한다.
	void GetPendingLedgerEntries(TArray<FWarriorAccountLedgerEntry>& OutEntries) const;

	//서버가 확인한 변경 내역을 확정 처리한다
	void MarkLedgerConfirmed(const TArray<FGuid>& InEntryIds);

	//외부 재화 잔액. 서버가 내려 준 값만 있다(없으면 0)
	int64 GetExternalCurrency(FName InCurrency) const;

	//서버가 알려 준 외부 재화 잔액을 반영한다. 변경 내역에도 남긴다.
	//TODO(server): 스탯 투자에 외부 재화를 쓰는 기획이 확정되면, InvestStatPoint를 "서버에 투자 요청 → 응답 스냅샷 적용"으로 바꾼다.
	void SetExternalCurrencyFromServer(FName InCurrency, int64 InBalance);

	//~ 알림
	UPROPERTY(BlueprintAssignable, Category = "Warrior|Account")
	FOnWarriorAccountChanged OnAccountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Warrior|Account")
	FOnWarriorStageRewarded OnStageRewarded;

	const FRules& GetRules() const { return Rules; }

private:
	//통계 서브시스템의 OnStageRecorded에 연결. 스테이지가 끝나 기록이 확정되면 온다
	UFUNCTION()
	void HandleStageRecorded(const FWarriorStageRecord& InStageRecord);

	void BuildDefinitions();
	const FStatDefinition* FindStatDefinition(const FGameplayTag& InStatTag) const;

	//경험치를 더하고 레벨 업을 처리한다. 올라간 레벨 수와 받은 포인트는 OutReward에 채운다
	void GrantExperience(int32 InExp, FWarriorStageReward& OutReward);

	void AddLedgerEntry(EWarriorAccountLedgerType InType, FName InCurrency, int64 InDelta, const FGameplayTag& InTarget = FGameplayTag(), const FGuid& InSourceId = FGuid());

	void BroadcastAccountChanged();

	UPROPERTY(Transient)
	FWarriorAccountData Data;

	UPROPERTY(Transient)
	TArray<FWarriorAccountLedgerEntry> Ledger;

	UPROPERTY(Transient)
	FWarriorStageReward LastReward;

	bool bHasLastReward = false;

	FRules Rules;

	TArray<FStatDefinition> StatDefinitions;

	//스킬 해금 비용(스탯 포인트)
	TMap<FGameplayTag, int32> SkillCosts;

	TWeakObjectPtr<UWarriorProfileStatsSubsystem> BoundStats;
};
