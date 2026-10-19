// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "WarriorTutorialZone.generated.h"

class AWarriorAICharacter;
class AWarriorBaseCharacter;
class UArrowComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FGameplayEventData;

//구역에 부르는 연습 상대 한 줄
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorTutorialSpawn
{
	GENERATED_BODY()

	//기존 적 BP를 복제한 것(C++ 클래스에서 바로 만든 BP는 ALS MovementModel이 비어 있다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	TSubclassOf<AWarriorAICharacter> EnemyClass;

	//구역 기준 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (MakeEditWidget = "true"))
	FTransform RelativeTransform;

	//true면 AttackPower를 0으로 둔다. 대미지 = BaseDamage × AttackPower ÷ Defense라 0이면 피해가 나지 않고, 막기·회피 판정과 피격 반응은 그대로다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	bool bHarmless = true;

	//허수아비: 행동(BT)을 멈춰 움직이지도 공격하지도 않는다. 맞기·처형 판정은 그대로(공격 연습)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	bool bStationary = false;

	//시작 체력 비율(1 = 가득). 처형 연습처럼 한 번에 쓰러질 상대를 둘 때 낮춘다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HealthRatio = 1.f;

	//시작 균형 비율(1 = 가득). 균형이 0이 되면 무너진다(Shared.Event.Stagger). 아주 낮추면 완벽한 패링 한 번에 무너진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float BalanceRatio = 1.f;

	//죽으면 RespawnDelay 뒤 다시 부른다(처형·카운터 연습)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	bool bRespawnWhenDead = true;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWarriorTutorialZoneReached, class AWarriorTutorialZone*);
//구역이 부른 연습 상대가 게임플레이 이벤트를 받았다(예: Shared.Event.Stagger)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWarriorTutorialZoneEnemyEvent, class AWarriorTutorialZone*, FGameplayTag);

/**
 * 튜토리얼 레벨에 단계 구역마다 1개 둔다(계획서 4-1). 단계의 ZoneId와 같은 이름으로 찾는다.
 * 단계가 시작되면 플레이어를 시작 위치로 옮기고 연습 상대를 부른다. 도착 볼륨에 플레이어가 들어오면 알린다.
 * 흐름(언제 부르고 치울지)은 AWarriorTutorialGameMode가 정한다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorTutorialZone : public AActor
{
	GENERATED_BODY()

public:
	AWarriorTutorialZone();

	FName GetZoneId() const { return ZoneId; }

	//이 구역의 단계를 시작한다. bTeleportPlayer면 플레이어를 옮기고, 연습 상대가 없으면 부른다
	void ActivateZone(APawn* InPlayerPawn);

	//연습 상대를 모두 치우고 다시 부르지 않는다
	void DeactivateZone();

	bool IsZoneActive() const { return bZoneActive; }

	//도착 지점 표시(GoalMarker)를 보이거나 숨긴다. 크기는 도착 볼륨(GoalVolume)을 따른다. 지점 도착 단계 동안만 켠다
	void SetGoalMarkerVisible(bool bInVisible);

	//구역이 켜진 동안 플레이어가 도착 볼륨에 들어왔다
	FOnWarriorTutorialZoneReached OnZoneReached;

	//구역이 켜진 동안 연습 상대가 게임플레이 이벤트를 받았다
	FOnWarriorTutorialZoneEnemyEvent OnEnemyEvent;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Warrior|Tutorial")
	FName ZoneId;

	//단계 시작 때 플레이어를 PlayerStartPoint로 옮긴다
	UPROPERTY(EditAnywhere, Category = "Warrior|Tutorial")
	bool bTeleportPlayer = true;

	UPROPERTY(EditAnywhere, Category = "Warrior|Tutorial")
	TArray<FWarriorTutorialSpawn> Enemies;

	UPROPERTY(EditAnywhere, Category = "Warrior|Tutorial", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 2.f;

	//사망 태그가 붙고 이만큼 지나도 OnCharacterDied가 오지 않으면 죽은 것으로 보고 다시 부른다
	UPROPERTY(EditAnywhere, Category = "Warrior|Tutorial", meta = (ClampMin = "0.0", Units = "s"))
	float DeathFallbackSeconds = 4.f;

	UPROPERTY(VisibleAnywhere, Category = "Warrior|Tutorial")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Warrior|Tutorial")
	TObjectPtr<UArrowComponent> PlayerStartPoint;

	//ReachZone 단계의 도착 지점. 크기·위치는 레벨에서 맞춘다
	UPROPERTY(VisibleAnywhere, Category = "Warrior|Tutorial")
	TObjectPtr<UBoxComponent> GoalVolume;

	//도착 지점의 바닥 원판. 평소에는 게임에서 숨긴다(에디터에서는 보임)
	UPROPERTY(VisibleAnywhere, Category = "Warrior|Tutorial")
	TObjectPtr<UStaticMeshComponent> GoalMarker;

	UPROPERTY(EditAnywhere, Category = "Warrior|Tutorial")
	FLinearColor GoalMarkerColor = FLinearColor(1.f, 0.62f, 0.12f, 1.f);

private:
	UFUNCTION()
	void HandleGoalVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEnemyDied(AWarriorBaseCharacter* DeadCharacter);

	void SpawnEnemy(int32 InSpawnIndex);
	//Enemies[InSpawnIndex]의 대미지 0·시작 체력·시작 균형을 넣는다. 시작 데이터가 아직이면 끝난 뒤에
	void ApplySpawnSettings(AWarriorAICharacter* InEnemy, int32 InSpawnIndex) const;
	void HandleEnemyInitialized(const FGameplayTag InTag, int32 InNewCount, TWeakObjectPtr<AWarriorAICharacter> InEnemy, int32 InSpawnIndex) const;
	void HandleEnemyGameplayEvent(FGameplayTag InEventTag, const FGameplayEventData* InPayload);
	//사망 태그가 붙으면 DeathFallbackSeconds 뒤를 대비한다. 사망 연출이 OnCharacterDied를 끝내 보내지 않는 경우(블레이드 모드 등)에도 다시 나오게
	void HandleEnemyDeathTagChanged(const FGameplayTag InTag, int32 InNewCount, TWeakObjectPtr<AWarriorAICharacter> InEnemy, int32 InSpawnIndex);

	//Enemies와 같은 순서. 부른 적(없으면 nullptr)
	TArray<TWeakObjectPtr<AWarriorAICharacter>> SpawnedEnemies;
	//Enemies와 같은 순서. 죽었지만 아직 남아 있는 몸. 다시 부를 때 남아 있으면 치운다
	TArray<TWeakObjectPtr<AWarriorAICharacter>> DyingEnemies;
	TArray<FTimerHandle> RespawnTimers;
	TWeakObjectPtr<APawn> PlayerPawn;
	bool bZoneActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GoalMarkerMaterial;
};
