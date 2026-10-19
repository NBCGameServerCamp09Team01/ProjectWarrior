#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WarriorStatTypes.h"
#include "WarriorStatsLibrary.generated.h"

class UGameplayAbility;
class UWarriorProfileStatsSubsystem;
class UWarriorStageStatsSubsystem;

/**
 * 통계 기록 창구 (S1).
 * 다른 시스템은 이 헤더 하나만 include하고 Record* 함수를 부르면 된다.
 * 스테이지 기록 중이 아니면(스테이지가 아닌 레벨, 클라이언트, 스테이지 종료 후) 아무 일도 일어나지 않는다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ 적 (웨이브 스포너)
	/** 적이 스폰되었을 때. 적 종류별 스폰 수와 처치 시간 계산에 쓰인다 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	static void RecordEnemySpawned(AActor* Enemy);

	/** 적이 죽었을 때(사망 신호). DeathType은 GetDeathTypeName으로 구할 수 있다 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	static void RecordEnemyKilled(AActor* Enemy, FName DeathType);

	//~ 전투 (AttributeSet, 공격 어빌리티)
	/**
	 * 데미지가 들어간 직후. Damage는 실제로 깎인 체력, Overkill은 남은 체력보다 초과한 양.
	 * 플레이어→적이면 공격 통계, 적→플레이어면 피격 통계로 들어간다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	static void RecordDamage(const FGameplayEffectContextHandle& EffectContext, AActor* Target, float Damage, float Overkill, bool bFatal);

	/** 자세(밸런스) 데미지가 들어간 직후 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	static void RecordBalanceDamage(const FGameplayEffectContextHandle& EffectContext, AActor* Target, float Amount);

	/** 회복이 들어간 직후. Healed는 실제로 오른 체력, Overheal은 최대 체력을 넘은 양 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	static void RecordHeal(const FGameplayEffectContextHandle& EffectContext, AActor* Target, float Healed, float Overheal);

	/** 플레이어 공격 어빌리티가 발동될 때 (공격 시도). 공격이 아닌 어빌리티에서는 부르지 않는다 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats")
	static void RecordAttackAttempt(const UGameplayAbility* Ability);

	//~ 경제·아이템
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static void RecordGoldEarned(const UObject* WorldContextObject, int32 Amount, FName Source);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static void RecordGoldSpent(const UObject* WorldContextObject, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static void RecordPotionUsed(const UObject* WorldContextObject, FName ItemId);

	/** 상점 구매 성공 시. 골드 사용량은 인벤토리 골드 감소로 따로 기록되므로 여기서는 구매 이력만 남긴다 */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static void RecordPurchase(const UObject* WorldContextObject, FName ItemId, int32 Count, int32 GoldSpent);

	//~ 범용
	/** 명시 필드 밖의 추가 통계 (WarriorStatTags). DimensionKey는 세부 분류, 없으면 None */
	UFUNCTION(BlueprintCallable, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static void RecordStat(const UObject* WorldContextObject, FGameplayTag StatTag, double Value, FName DimensionKey);

	//~ 조회 (BP·Python·이후 UI)
	/** 이번 스테이지 기록기. 스테이지가 아닌 레벨에서도 객체는 있지만 IsRecording이 false */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static UWarriorStageStatsSubsystem* GetStageStats(const UObject* WorldContextObject);

	/** GameInstance 통계 보관소 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats", meta = (WorldContext = "WorldContextObject"))
	static UWarriorProfileStatsSubsystem* GetProfileStats(const UObject* WorldContextObject);

	//~ 계산 (저장하지 않는 파생 값)
	/** 공격 비율 = 공격 횟수 / 공격 시도 횟수 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	static double GetHitRate(const FWarriorAttackStats& Attack) { return Attack.GetHitRate(); }

	/** 평균 데미지 = 가한 데미지 / 적중 타격 수 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	static double GetAverageDamage(const FWarriorAttackStats& Attack) { return Attack.GetAverageDamage(); }

	/** 적 종류별 평균 처치 시간 */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	static double GetAverageTimeToKill(const FWarriorEnemyTypeStats& Enemy) { return Enemy.GetAverageTimeToKill(); }

	/** Extra 통계 값. 없으면 false */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	static bool FindExtraStat(const FWarriorStatBlock& Stats, FGameplayTag StatTag, FName DimensionKey, FWarriorStatValue& OutValue);

	//~ 이름 도우미
	/** 적 종류 이름 (BP 클래스 이름에서 _C를 뺀 것) */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	static FName GetEnemyTypeName(const AActor* Enemy);

	/** 사망 태그로 판단한 사망 방식: Normal / Knockback / Finisher, 태그가 없으면 None */
	UFUNCTION(BlueprintPure, Category = "Warrior|Stats")
	static FName GetDeathTypeName(AActor* DeadActor);
};
