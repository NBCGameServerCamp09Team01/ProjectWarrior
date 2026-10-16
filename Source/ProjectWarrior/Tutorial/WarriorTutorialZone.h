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

	//죽으면 RespawnDelay 뒤 다시 부른다(처형·카운터 연습)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Warrior|Tutorial")
	bool bRespawnWhenDead = true;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWarriorTutorialZoneReached, class AWarriorTutorialZone*);

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
	void MakeHarmless(AWarriorAICharacter* InEnemy) const;
	void HandleEnemyInitialized(const FGameplayTag InTag, int32 InNewCount, TWeakObjectPtr<AWarriorAICharacter> InEnemy) const;

	//Enemies와 같은 순서. 부른 적(없으면 nullptr)
	TArray<TWeakObjectPtr<AWarriorAICharacter>> SpawnedEnemies;
	TArray<FTimerHandle> RespawnTimers;
	TWeakObjectPtr<APawn> PlayerPawn;
	bool bZoneActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GoalMarkerMaterial;
};
