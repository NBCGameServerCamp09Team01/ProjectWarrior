// Fill out your copyright notice in the Description page of Project Settings.


#include "CameraClipHideComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UCameraClipHideComponent::UCameraClipHideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 카메라 위치가 확정된 뒤 검사
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCameraClipHideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;

	// 로컬 플레이어 화면에서만 처리
	if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();

	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Character = *It;

		if (!Character || (!bIncludeOwner && Character == OwnerPawn))
		{
			continue;
		}

		const bool bHidden = HiddenCharacters.Contains(Character);

		if (FVector::DistSquared(Character->GetActorLocation(), CameraLocation) > FMath::Square(CheckRadius))
		{
			if (bHidden)
			{
				ShowCharacter(Character);
			}
			continue;
		}

		const float Distance = ComputeDistanceToCharacter(Character, CameraLocation);

		if (!bHidden && Distance <= HideDistance)
		{
			HideCharacter(Character);
		}
		else if (bHidden && Distance > ShowDistance)
		{
			ShowCharacter(Character);
		}
	}

	// 사라진 캐릭터 정리
	for (auto It = HiddenCharacters.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void UCameraClipHideComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<TWeakObjectPtr<ACharacter>> Characters;
	HiddenCharacters.GetKeys(Characters);

	for (const TWeakObjectPtr<ACharacter>& Character : Characters)
	{
		if (Character.IsValid())
		{
			ShowCharacter(Character.Get());
		}
	}

	HiddenCharacters.Reset();

	Super::EndPlay(EndPlayReason);
}

float UCameraClipHideComponent::ComputeDistanceToCharacter(const ACharacter* Character, const FVector& CameraLocation)
{
	// 피직스 에셋 바디 기준 (팔·가시 등 캡슐 밖으로 튀어나온 부분 포함). 안에 있으면 0
	if (const USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh && Mesh->IsCollisionEnabled())
	{
		FVector ClosestPoint;
		const float Distance = Mesh->GetDistanceToCollision(CameraLocation, ClosestPoint);

		if (Distance >= 0.f)
		{
			return Distance;
		}
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();

	if (!Capsule)
	{
		return TNumericLimits<float>::Max();
	}

	// 캡슐 표면까지의 거리 (안이면 0)
	const FVector Local = Capsule->GetComponentTransform().InverseTransformPositionNoScale(CameraLocation);
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfSegment = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() - Radius, 0.f);
	const FVector SegmentPoint(0.f, 0.f, FMath::Clamp(Local.Z, -HalfSegment, HalfSegment));

	return FMath::Max(FVector::Dist(Local, SegmentPoint) - Radius, 0.f);
}

void UCameraClipHideComponent::HideCharacter(ACharacter* Character)
{
	USkeletalMeshComponent* Mesh = Character->GetMesh();

	if (!Mesh)
	{
		return;
	}

	// 메시와 메시에 붙은 것(무기 등) 중 지금 보이는 것만 숨기고 기억
	TArray<USceneComponent*> Components;
	Mesh->GetChildrenComponents(true, Components);
	Components.Add(Mesh);

	TArray<TWeakObjectPtr<UPrimitiveComponent>>& HiddenComponents = HiddenCharacters.Add(Character);

	for (USceneComponent* Component : Components)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);

		if (Primitive && Primitive->IsVisible())
		{
			Primitive->SetVisibility(false, false);
			HiddenComponents.Add(Primitive);
		}
	}
}

void UCameraClipHideComponent::ShowCharacter(ACharacter* Character)
{
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenComponents;

	if (!HiddenCharacters.RemoveAndCopyValue(Character, HiddenComponents))
	{
		return;
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : HiddenComponents)
	{
		if (Primitive.IsValid())
		{
			Primitive->SetVisibility(true, false);
		}
	}
}
