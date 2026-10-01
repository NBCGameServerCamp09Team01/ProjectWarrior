// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectWarrior/Types/WarriorAttackAreaTypes.h"
#include "WarriorAttackIndicator.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 범위 공격의 위험 범위를 바닥 데칼로 표시하는 액터. 액터 위치·방향 = 판정 영역의 원점·정면.
 *
 * 데칼 머티리얼(Deferred Decal)에 넘기는 스칼라 파라미터
 *  - ProgressParameterName  : 0 -> 1 로 FillDuration 동안 채워짐 (공격 도달까지 남은 시간 표현)
 *  - ShapeParameterName     : 0 = Circle, 1 = Box, 2 = Cone
 *  - ConeAngleParameterName : Cone 전체 각도(도)
 * Circle/Cone은 데칼 UV 중심이 원점, Box는 UV 한쪽 끝이 원점(정면으로 뻗음)이다.
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorAttackIndicator : public AActor
{
	GENERATED_BODY()

public:
	AWarriorAttackIndicator();

	// 영역 모양에 맞춰 데칼 크기와 머티리얼 파라미터를 설정하고 채우기를 시작
	UFUNCTION(BlueprintCallable, Category = "Warrior|AttackIndicator")
	void InitializeIndicator(const FWarriorAttackAreaData& InAreaData, float InFillDuration);

	UFUNCTION(BlueprintPure, Category = "Warrior|AttackIndicator")
	float GetFillProgress() const;

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AttackIndicator")
	USceneComponent* IndicatorRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AttackIndicator")
	UDecalComponent* IndicatorDecal;

	// 비워 두면 IndicatorDecal에 지정된 머티리얼 사용
	UPROPERTY(EditDefaultsOnly, Category = "AttackIndicator")
	TObjectPtr<UMaterialInterface> IndicatorMaterial;

	// 데칼 투영 깊이(위아래 절반씩). 경사·단차가 있으면 늘림
	UPROPERTY(EditDefaultsOnly, Category = "AttackIndicator", meta = (ClampMin = "1.0"))
	float DecalProjectionDepth = 300.f;

	// 제거가 누락됐을 때를 대비한 안전 수명. FillDuration 뒤 이 시간이 지나면 스스로 제거 (0 = 사용 안 함)
	UPROPERTY(EditDefaultsOnly, Category = "AttackIndicator", meta = (ClampMin = "0.0"))
	float SafetyLifeSpanAfterFill = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "AttackIndicator|Material")
	FName ProgressParameterName = TEXT("Progress");

	UPROPERTY(EditDefaultsOnly, Category = "AttackIndicator|Material")
	FName ShapeParameterName = TEXT("Shape");

	UPROPERTY(EditDefaultsOnly, Category = "AttackIndicator|Material")
	FName ConeAngleParameterName = TEXT("ConeAngle");

private:
	void SetFillProgress(float InProgress);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMID;

	float FillDuration = 0.f;
	float ElapsedTime = 0.f;
};
