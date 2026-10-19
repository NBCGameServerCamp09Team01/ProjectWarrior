// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorBaseCharacter.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "MotionWarpingComponent.h"
#include "ProjectWarrior/Characters/WarriorCharacterMovementComponent.h"
#include "TimerManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

AWarriorBaseCharacter::AWarriorBaseCharacter(const FObjectInitializer& ObjectInitializer)
	// 이동 속도 배율(수식어 신속·둔화)을 최종 최고 속도에 곱하는 컴포넌트. ALS가 요청하는 UALSCharacterMovementComponent의 자식이라 이쪽이 쓰인다
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UWarriorCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	//캐릭터 메쉬는 데칼의 영향을 받지 않음
	GetMesh()->bReceivesDecals = false;

	WarriorAbilitySystemComponent = CreateDefaultSubobject<UWarriorAbilitySystemComponent>(TEXT("WarriorAbilitySystemComponent"));

	WarriorAttributeSet = CreateDefaultSubobject<UWarriorAttributeSet>(TEXT("WarriorAttributeSet"));

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

UAbilitySystemComponent* AWarriorBaseCharacter::GetAbilitySystemComponent() const
{
	return GetWarriorAbilitySystemComponent();
}

UPawnCombatComponent* AWarriorBaseCharacter::GetPawnCombatComponent() const
{
	return nullptr;
}

UPawnUIComponent* AWarriorBaseCharacter::GetPawnUIComponent() const
{
	return nullptr;
}

void AWarriorBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (WarriorAbilitySystemComponent)
	{
		WarriorAbilitySystemComponent->InitAbilityActorInfo(this, this);

		ensureMsgf(!CharacterStartUpData.IsNull(), TEXT("Forgot to assign start up data to %s"), *GetName());
	}
}

void AWarriorBaseCharacter::OnCharacterDiedEvent_Implementation()
{
	OnCharacterDied.Broadcast(this);
}

void AWarriorBaseCharacter::SetMovementSpeedScale(FName InKey, float InScale)
{
	if (InKey.IsNone())
	{
		return;
	}

	if (InScale <= 0.f || FMath::IsNearlyEqual(InScale, 1.f))
	{
		MovementSpeedScales.Remove(InKey);
	}
	else
	{
		MovementSpeedScales.Add(InKey, InScale);
	}
	RefreshMovementSpeedScale();
}

void AWarriorBaseCharacter::ApplyTimedMovementSpeedScale(FName InKey, float InScale, float InDuration)
{
	if (InKey.IsNone() || InDuration <= 0.f)
	{
		return;
	}

	SetMovementSpeedScale(InKey, InScale);

	FTimerHandle& TimerHandle = MovementSpeedScaleTimers.FindOrAdd(InKey);
	GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, InKey]()
	{
		MovementSpeedScaleTimers.Remove(InKey);
		SetMovementSpeedScale(InKey, 1.f);
	}), InDuration, false);
}

float AWarriorBaseCharacter::GetMovementSpeedScale() const
{
	float Scale = 1.f;
	for (const TPair<FName, float>& Pair : MovementSpeedScales)
	{
		Scale *= Pair.Value;
	}
	return Scale;
}

void AWarriorBaseCharacter::ShowTimedStatusEffect(FName InKey, UNiagaraSystem* InSystem, const FLinearColor& InColor, float InDuration)
{
	if (InKey.IsNone() || !InSystem || InDuration <= 0.f)
	{
		return;
	}

	TObjectPtr<UNiagaraComponent>& Component = StatusEffectComponents.FindOrAdd(InKey);
	if (!IsValid(Component))
	{
		// 메시를 붙은 부모에서 찾는 이펙트라 보이는 스켈레탈 메시에 붙인다(ALS 적은 GetMesh()가 숨겨진 애니메이션용)
		TArray<USkeletalMeshComponent*> Meshes;
		GetComponents<USkeletalMeshComponent>(Meshes);
		USkeletalMeshComponent* const* Parent = Meshes.FindByPredicate([](const USkeletalMeshComponent* MeshComponent)
		{
			return MeshComponent && MeshComponent->IsVisible() && MeshComponent->GetSkeletalMeshAsset();
		});
		if (!Parent)
		{
			StatusEffectComponents.Remove(InKey);
			return;
		}
		Component = UNiagaraFunctionLibrary::SpawnSystemAttached(InSystem, *Parent, NAME_None, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
		if (!Component)
		{
			StatusEffectComponents.Remove(InKey);
			return;
		}
	}
	Component->SetVariableLinearColor(TEXT("User.AuraColor"), InColor);

	FTimerHandle& TimerHandle = StatusEffectTimers.FindOrAdd(InKey);
	GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, InKey]()
	{
		StatusEffectTimers.Remove(InKey);
		TObjectPtr<UNiagaraComponent> Finished;
		if (StatusEffectComponents.RemoveAndCopyValue(InKey, Finished) && IsValid(Finished))
		{
			Finished->DestroyComponent();
		}
	}), InDuration, false);
}

void AWarriorBaseCharacter::RefreshMovementSpeedScale()
{
	// MaxWalkSpeed(ALS·락온 등이 정함)는 건드리지 않고 최종 최고 속도에만 곱한다
	if (UWarriorCharacterMovementComponent* MovementComponent = Cast<UWarriorCharacterMovementComponent>(GetCharacterMovement()))
	{
		MovementComponent->SetSpeedScale(GetMovementSpeedScale());
	}
}
