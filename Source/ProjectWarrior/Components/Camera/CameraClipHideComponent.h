// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraClipHideComponent.generated.h"

class ACharacter;
class UPrimitiveComponent;

/**
 * 카메라가 캐릭터 몸 안(또는 아주 가까이)에 들어가면 그 캐릭터의 메시를 숨겨 클리핑(잘린 폴리곤)이 보이지 않게 한다.
 * 플레이어에 붙여 사용. 매 프레임 화면 카메라 위치와 각 캐릭터 메시의 충돌체(피직스 에셋) 거리를 검사하고,
 * 빠져나오면 숨겼던 컴포넌트만 다시 보이게 한다.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class PROJECTWARRIOR_API UCameraClipHideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraClipHideComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 카메라와 메시 충돌체 사이가 이 거리보다 가까우면 숨김 (카메라 근평면 여유)
	UPROPERTY(EditDefaultsOnly, Category = "CameraClip", meta = (ClampMin = "0.0", Units = "cm"))
	float HideDistance = 25.f;

	// 숨긴 뒤 다시 보이게 할 거리 (깜빡임 방지용으로 HideDistance보다 조금 크게)
	UPROPERTY(EditDefaultsOnly, Category = "CameraClip", meta = (ClampMin = "0.0", Units = "cm"))
	float ShowDistance = 40.f;

	// 이 거리보다 먼 캐릭터는 검사하지 않음
	UPROPERTY(EditDefaultsOnly, Category = "CameraClip", meta = (ClampMin = "0.0", Units = "cm"))
	float CheckRadius = 1500.f;

	// 이 컴포넌트를 가진 플레이어 자신도 검사 (벽 때문에 카메라가 몸에 붙을 때)
	UPROPERTY(EditDefaultsOnly, Category = "CameraClip")
	bool bIncludeOwner = true;

private:
	// 카메라에서 캐릭터 메시 충돌체까지의 거리. 충돌체가 없으면 캡슐로 계산
	static float ComputeDistanceToCharacter(const ACharacter* Character, const FVector& CameraLocation);

	void HideCharacter(ACharacter* Character);

	void ShowCharacter(ACharacter* Character);

	// 숨긴 캐릭터와, 그때 보이던 상태라서 숨긴 컴포넌트들 (다시 보이게 할 대상)
	TMap<TWeakObjectPtr<ACharacter>, TArray<TWeakObjectPtr<UPrimitiveComponent>>> HiddenCharacters;
};
