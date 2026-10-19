// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraClipHideComponent.generated.h"

class ACharacter;
class UMaterialInterface;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ECameraClipHandling : uint8
{
	// 반투명 유령 머티리얼로 교체 (뒤 배경이 비쳐 보임)
	Ghost,
	// 메시를 숨김
	Hide
};

// 처리 중인 컴포넌트 하나의 원래 상태 (머티리얼은 GC되지 않도록 UPROPERTY로 보관)
USTRUCT()
struct FCameraClipComponentState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Component;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	// 머티리얼 교체 대신 숨겼는지
	UPROPERTY()
	bool bHidden = false;
};

USTRUCT()
struct FCameraClipCharacterState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCameraClipComponentState> Components;
};

/**
 * 카메라가 캐릭터 몸 안(또는 아주 가까이)에 들어가면 그 캐릭터를 반투명 유령 머티리얼로 바꾸거나 숨겨 클리핑(잘린 폴리곤)이 보이지 않게 한다.
 * 플레이어에 붙여 사용. 매 프레임 화면 카메라 위치와 각 캐릭터 메시의 충돌체(피직스 에셋) 거리를 검사하고,
 * 빠져나오면 바꾼 컴포넌트만 원래대로 되돌린다.
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

	UPROPERTY(EditDefaultsOnly, Category = "CameraClip")
	ECameraClipHandling Handling = ECameraClipHandling::Ghost;

	// Ghost 방식에서 메시 전체에 씌울 반투명 머티리얼 (스켈레탈 메시 사용 허용 필요)
	UPROPERTY(EditDefaultsOnly, Category = "CameraClip", meta = (EditCondition = "Handling == ECameraClipHandling::Ghost"))
	TObjectPtr<UMaterialInterface> GhostMaterial;

	// 카메라와 메시 충돌체 사이가 이 거리보다 가까우면 처리 (카메라 근평면 여유)
	UPROPERTY(EditDefaultsOnly, Category = "CameraClip", meta = (ClampMin = "0.0", Units = "cm"))
	float HideDistance = 25.f;

	// 처리한 뒤 원래대로 되돌릴 거리 (깜빡임 방지용으로 HideDistance보다 조금 크게)
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

	// 처리 중인 캐릭터와, 바꾼 컴포넌트들의 원래 상태
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<ACharacter>, FCameraClipCharacterState> HiddenCharacters;
};
