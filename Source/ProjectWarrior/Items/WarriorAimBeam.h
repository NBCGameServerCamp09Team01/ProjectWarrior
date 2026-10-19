// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WarriorAimBeam.generated.h"

class UNiagaraComponent;

/**
 * 저격 조준선. 시작점(활 소켓)에서 대상까지 나이아가라 빔을 매 프레임 갱신한다.
 * 조준 중에는 대상을 따라가고, LockBeam 이후에는 색을 바꾼다 (지금 피하라는 신호). 끝점 고정은 선택.
 *
 * 나이아가라 시스템에 필요한 유저 파라미터 (이름은 아래 프로퍼티로 변경 가능)
 *  - BeamEnd   (Vector, 월드 좌표): 빔 끝점. 빔 시작점은 시스템 위치(= 이 액터 위치)
 *  - BeamColor (Linear Color): 빔 색
 */
UCLASS()
class PROJECTWARRIOR_API AWarriorAimBeam : public AActor
{
	GENERATED_BODY()

public:
	AWarriorAimBeam();

	// 빔 시작점을 InSourceComponent의 소켓에 붙이고 InTarget을 따라가기 시작
	UFUNCTION(BlueprintCallable, Category = "Warrior|AimBeam")
	void InitializeBeam(USceneComponent* InSourceComponent, FName InSourceSocket, AActor* InTarget);

	// 고정 색으로 바꿈. bFreezeEnd면 끝점도 현재 위치에 고정하고, 아니면 계속 대상을 따라감
	UFUNCTION(BlueprintCallable, Category = "Warrior|AimBeam")
	void LockBeam(bool bFreezeEnd = true);

	UFUNCTION(BlueprintPure, Category = "Warrior|AimBeam")
	bool IsLocked() const { return bLocked; }

protected:
	virtual void Tick(float DeltaSeconds) override;

	// 고정될 때 BP에서 소리·이펙트를 추가할 수 있도록 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|AimBeam", meta = (DisplayName = "On Beam Locked"))
	void BP_OnBeamLocked();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AimBeam")
	TObjectPtr<UNiagaraComponent> BeamComponent;

	UPROPERTY(EditDefaultsOnly, Category = "AimBeam")
	FName BeamEndParameterName = FName("BeamEnd");

	UPROPERTY(EditDefaultsOnly, Category = "AimBeam")
	FName BeamColorParameterName = FName("BeamColor");

	UPROPERTY(EditDefaultsOnly, Category = "AimBeam")
	FLinearColor TrackingColor = FLinearColor(1.f, 0.55f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "AimBeam")
	FLinearColor LockedColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

	// 대상 액터 원점에서 끝점까지의 오프셋. 저격 화살은 대상 원점(캡슐 중심)을 향하므로 기본 0 = 실제 탄도와 같음
	UPROPERTY(EditDefaultsOnly, Category = "AimBeam")
	FVector TargetOffset = FVector::ZeroVector;

	// 대상이 없을 때 시작점 정면으로 뻗는 길이
	UPROPERTY(EditDefaultsOnly, Category = "AimBeam", meta = (ClampMin = "0.0", Units = "cm"))
	float FallbackLength = 1500.f;

private:
	void UpdateBeamEnd();

	TWeakObjectPtr<AActor> Target;
	FVector LockedEnd = FVector::ZeroVector;
	bool bLocked = false;
	bool bEndFrozen = false;
};
