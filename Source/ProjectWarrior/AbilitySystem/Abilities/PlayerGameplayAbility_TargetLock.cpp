// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_TargetLock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectWarrior/Widgets/WarriorWidgetBase.h"
#include "ProjectWarrior/Controllers/WarriorPlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"

void UPlayerGameplayAbility_TargetLock::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	TryLockOnTarget();

	InitTargetLockMovement();
	InitTargetLockMappingContext();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UPlayerGameplayAbility_TargetLock::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ResetTargetLockMovement();
	ResetTargetLockMappingContext();

	CleanUp();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerGameplayAbility_TargetLock::OnTargetLockTick(float DeltaTime)
{
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Shared.Status.Death")));

	if (!CurrentLockedActor 
		|| UWarriorFunctionLibrary::NativeDoesActorHaveTag(CurrentLockedActor, WarriorGameplayTags::Shared_Ability_Death) 
		|| UWarriorFunctionLibrary::NativeDoesActorHaveAnyTag(GetPlayerCharacterFromActorInfo(), TagContainer)
		)
	{
		CancelTargetLockAbility();

		return;
	}

	SetTargetLockWidgetPosition();

	const bool bShouldOverrideRotationController = !UWarriorFunctionLibrary::NativeDoesActorHaveTag(GetPlayerCharacterFromActorInfo(), WarriorGameplayTags::Player_Status_OverrideLockOnRotation_Controller);
	const bool bShouldOverrideRotationCharacter = !UWarriorFunctionLibrary::NativeDoesActorHaveTag(GetPlayerCharacterFromActorInfo(), WarriorGameplayTags::Player_Status_OverrideLockOnRotation_Character);

	FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation
	(
		GetPlayerCharacterFromActorInfo()->GetActorLocation(),
		CurrentLockedActor->GetActorLocation()
	);

	LookAtRot -= FRotator(TargetLockCameraOffsetDistance, 0.f, 0.f);

	const FRotator CurrentControlRot = GetPlayerControllerFromActorInfo()->GetControlRotation();
	const FRotator TargetRot = FMath::RInterpTo(CurrentControlRot, LookAtRot, DeltaTime, TargetLockRotationInterpSpeed);

	if (bShouldOverrideRotationController)
	{
		GetPlayerControllerFromActorInfo()->SetControlRotation(FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.f));
	}

	if (bShouldOverrideRotationCharacter)
	{
		GetPlayerCharacterFromActorInfo()->SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
	}
}

void UPlayerGameplayAbility_TargetLock::SwitchTarget(const FGameplayTag& InSwitchDirectionTag)
{
	GetAvailableActorsToLock();

	TArray<AActor*> ActorsOnLeft;
	TArray<AActor*> ActorsOnRight;
	AActor* NewTargetToLock = nullptr;

	GetAvailableActorsAroundTarget(ActorsOnLeft, ActorsOnRight);

	if (InSwitchDirectionTag == WarriorGameplayTags::Player_Event_SwitchTarget_Left)
	{
		NewTargetToLock = GetNearestTargetFromAvailableActors(ActorsOnLeft);
	}
	else
	{
		NewTargetToLock = GetNearestTargetFromAvailableActors(ActorsOnRight);
	}

	if (NewTargetToLock)
	{
		CurrentLockedActor = NewTargetToLock;
		GetPlayerCharacterFromActorInfo()->SetCurrentLockedActor(CurrentLockedActor);
	}
}

void UPlayerGameplayAbility_TargetLock::TryLockOnTarget()
{
	GetAvailableActorsToLock();


	if (AvailableActorsToLock.IsEmpty())
	{
		CancelTargetLockAbility();
		return;
	}

	CurrentLockedActor = GetNearestTargetFromAvailableActors(AvailableActorsToLock);

	if (CurrentLockedActor)
	{
		DrawTargetLockWidget();

		GetPlayerCharacterFromActorInfo()->SetCurrentLockedActor(CurrentLockedActor);

		SetTargetLockWidgetPosition();
	}
	else
	{
		CancelTargetLockAbility();
	}
}

void UPlayerGameplayAbility_TargetLock::GetAvailableActorsToLock()
{
	AvailableActorsToLock.Empty();
	TArray<FHitResult> BoxTraceHits;

	UKismetSystemLibrary::BoxTraceMultiForObjects(
		GetPlayerCharacterFromActorInfo(),
		GetPlayerCharacterFromActorInfo()->GetActorLocation(),
		GetPlayerCharacterFromActorInfo()->GetActorLocation() + GetPlayerCharacterFromActorInfo()->GetActorForwardVector() * BoxTraceDistance,
		TraceBoxSize / 2.f,
		GetPlayerCharacterFromActorInfo()->GetActorForwardVector().ToOrientationRotator(),
		BoxTraceChannel,
		false,
		TArray<AActor*>(),
		bShowPersistentDebugShape ? EDrawDebugTrace::Persistent : EDrawDebugTrace::None,
		BoxTraceHits,
		true
	);

	for (const FHitResult& TraceHit : BoxTraceHits)
	{
		if (AActor* HitActor = TraceHit.GetActor())
		{
			if (HitActor != GetPlayerCharacterFromActorInfo())
			{
				AvailableActorsToLock.AddUnique(HitActor);
			}
		}
	}
}

AActor* UPlayerGameplayAbility_TargetLock::GetNearestTargetFromAvailableActors(const TArray<AActor*>& InAvailableActors)
{
	float ClosestDistance = 0.f;
	return UGameplayStatics::FindNearestActor(GetPlayerCharacterFromActorInfo()->GetActorLocation(), InAvailableActors, ClosestDistance);
}

void UPlayerGameplayAbility_TargetLock::GetAvailableActorsAroundTarget(TArray<AActor*>& OutActorsOnLeft, TArray<AActor*>& OutActorsOnRight)
{
	if (!CurrentLockedActor || AvailableActorsToLock.IsEmpty())
	{
		CancelTargetLockAbility();
		return;
	}

	const FVector PlayerLocation = GetPlayerCharacterFromActorInfo()->GetActorLocation();
	const FVector PlayerToCurrentNormalized = (CurrentLockedActor->GetActorLocation() - PlayerLocation).GetSafeNormal();

	for (AActor* AvailableActor : AvailableActorsToLock)
	{
		if (!AvailableActor || AvailableActor == CurrentLockedActor) continue;

		const FVector PlayerToAvailableNormalized = (AvailableActor->GetActorLocation() - PlayerLocation).GetSafeNormal();

		const FVector CrossResult = FVector::CrossProduct(PlayerToCurrentNormalized, PlayerToAvailableNormalized);

		if (CrossResult.Z > 0.f)
		{
			OutActorsOnRight.AddUnique(AvailableActor);
		}
		else
		{
			OutActorsOnLeft.AddUnique(AvailableActor);
		}
	}
}

void UPlayerGameplayAbility_TargetLock::DrawTargetLockWidget()
{
	if (!DrawnTargetLockWidget)
	{
		checkf(TargetLockWidgetClass, TEXT("Forgot to assign a valid widget class in Blueprint"));

		DrawnTargetLockWidget = CreateWidget<UWarriorWidgetBase>(GetPlayerControllerFromActorInfo(), TargetLockWidgetClass);

		check(DrawnTargetLockWidget);

		DrawnTargetLockWidget->AddToViewport();
	}
}

void UPlayerGameplayAbility_TargetLock::SetTargetLockWidgetPosition()
{
	if (!DrawnTargetLockWidget || !CurrentLockedActor)
	{
		CancelTargetLockAbility();
		return;
	}

	FVector2D ScreenPosition;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition
	(
		GetPlayerControllerFromActorInfo(),
		CurrentLockedActor->GetActorLocation(),
		ScreenPosition,
		true
	);

	if (TargetLockWidgetSize == FVector2D::ZeroVector)
	{
		DrawnTargetLockWidget->WidgetTree->ForEachWidget
		(
			[this](UWidget* FoundWidget)
			{
				if (USizeBox* FoundSizeBox = Cast<USizeBox>(FoundWidget))
				{
					TargetLockWidgetSize.X = FoundSizeBox->GetWidthOverride();
					TargetLockWidgetSize.Y = FoundSizeBox->GetHeightOverride();
				}
			}
		);
	}

	ScreenPosition -= (TargetLockWidgetSize / 2.f);

	DrawnTargetLockWidget->SetPositionInViewport(ScreenPosition, false);
}

void UPlayerGameplayAbility_TargetLock::CancelTargetLockAbility()
{
	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

void UPlayerGameplayAbility_TargetLock::CleanUp()
{
	AvailableActorsToLock.Empty();

	CurrentLockedActor = nullptr;

	GetPlayerCharacterFromActorInfo()->SetCurrentLockedActor(nullptr);

	if (DrawnTargetLockWidget)
	{
		DrawnTargetLockWidget->RemoveFromParent();
	}

	DrawnTargetLockWidget = nullptr;

	TargetLockWidgetSize = FVector2D::ZeroVector;
}

void UPlayerGameplayAbility_TargetLock::InitTargetLockMovement()
{
	GetPlayerCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed = TargetLockMaxWalkSpeed;
}

void UPlayerGameplayAbility_TargetLock::InitTargetLockMappingContext()
{
	const ULocalPlayer* LocalPlayer = GetPlayerControllerFromActorInfo()->GetLocalPlayer();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	check(Subsystem)

	Subsystem->AddMappingContext(TargetLockMappingContext, 3);
}

void UPlayerGameplayAbility_TargetLock::ResetTargetLockMovement()
{
	if (!UWarriorFunctionLibrary::NativeDoesActorHaveTag(GetPlayerCharacterFromActorInfo(), WarriorGameplayTags::Player_Status_Blocking))
	{
		GetPlayerCharacterFromActorInfo()->GetCharacterMovement()->MaxWalkSpeed = 375.f;
	}
}

void UPlayerGameplayAbility_TargetLock::ResetTargetLockMappingContext()
{
	if (!GetPlayerControllerFromActorInfo())
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = GetPlayerControllerFromActorInfo()->GetLocalPlayer();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	check(Subsystem)

	Subsystem->RemoveMappingContext(TargetLockMappingContext);
}
