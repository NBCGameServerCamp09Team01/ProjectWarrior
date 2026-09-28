// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ProjectWarrior/DataAssets/DataAsset_InputConfig.h"
#include "ProjectWarrior/Components/Input/WarriorInputComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/DataAssets/DataAsset_PlayerStartUpData.h"
#include "ProjectWarrior/Components/Combat/PlayerCombatComponent.h"
#include "ProjectWarrior/Components/UI/PlayerUIComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

AWarriorPlayerCharacter::AWarriorPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 200.f;
    CameraBoom->SocketOffset = FVector(0.f, 55.f, 65.f);
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    PlayerCombatComponent = CreateDefaultSubobject<UPlayerCombatComponent>(TEXT("PlayerCombatComponent"));

    PlayerUIComponent = CreateDefaultSubobject<UPlayerUIComponent>(TEXT("PlayerUIComponent"));
}

UPawnCombatComponent* AWarriorPlayerCharacter::GetPawnCombatComponent() const
{
    return PlayerCombatComponent;
}

UPawnUIComponent* AWarriorPlayerCharacter::GetPawnUIComponent() const
{
    return PlayerUIComponent;
}

UPlayerUIComponent* AWarriorPlayerCharacter::GetPlayerUIComponent() const
{
    return PlayerUIComponent;
}

void AWarriorPlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    //if (WarriorAbilitySystemComponent && WarriorAttributeSet)
    //{
    //    const FString ASCText = FString::Printf(TEXT("Owner Actor : %s, Avatar Actor : %s"),
    //        *WarriorAbilitySystemComponent->GetOwnerActor()->GetActorLabel(), *WarriorAbilitySystemComponent->GetAvatarActor()->GetActorLabel());

    //    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Ability System Component is Valid %s"), *ASCText));
    //}

    if (!CharacterStartUpData.IsNull())
    {
        if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous())
        {
            LoadedData->GiveToAbilitySystemComponent(WarriorAbilitySystemComponent);
        }
    }
}

void AWarriorPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    checkf(InputConfigData, TEXT("Forgot to assign a valid data asset as input config"));

    ULocalPlayer* LocalPlayer = GetController<APlayerController>()->GetLocalPlayer();

    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

    check(Subsystem);

    Subsystem->AddMappingContext(InputConfigData->DefaultMappingContext, 0);

    UWarriorInputComponent* WarriorInputComponent = CastChecked<UWarriorInputComponent>(PlayerInputComponent);

    WarriorInputComponent->BindNativeInputAction(InputConfigData, WarriorGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
    WarriorInputComponent->BindNativeInputAction(InputConfigData, WarriorGameplayTags::InputTag_Move, ETriggerEvent::Completed, this, &ThisClass::Input_MoveCompleted);
    WarriorInputComponent->BindNativeInputAction(InputConfigData, WarriorGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
    WarriorInputComponent->BindNativeInputAction(InputConfigData, WarriorGameplayTags::InputTag_LeftButton, ETriggerEvent::Started, this, &ThisClass::Input_LeftButton);
    WarriorInputComponent->BindNativeInputAction(InputConfigData, WarriorGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Triggered, this, &ThisClass::Input_SwitchTargetTriggered);
    WarriorInputComponent->BindNativeInputAction(InputConfigData, WarriorGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Completed, this, &ThisClass::Input_SwitchTargetCompleted);

    WarriorInputComponent->BindAbilityInputAction(InputConfigData, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

void AWarriorPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void AWarriorPlayerCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
    const FVector2D MovementVector = InputActionValue.Get<FVector2D>();
    LastMoveInputVector = MovementVector;

    const FRotator MovementRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);

    if (MovementState == EALSMovementState::Grounded || MovementState == EALSMovementState::InAir)
    {
        if (MovementVector.Y != 0.f)
        {
            const FVector ForwardDirection = MovementRotation.RotateVector(FVector::ForwardVector);

            AddMovementInput(ForwardDirection, MovementVector.Y);
        }

        if (MovementVector.X != 0.f)
        {
            const FVector RightDirection = MovementRotation.RotateVector(FVector::RightVector);

            AddMovementInput(RightDirection, MovementVector.X);
        }
    }
}

void AWarriorPlayerCharacter::Input_MoveCompleted(const FInputActionValue& InputActionValue)
{
    UWorld* World = GetWorld();
    check(World);

    const float PrevMoveInputTime = LastMoveInputTime;
    LastMoveInputTime = World->GetTimeSeconds();

    if (LastMoveInputTime - PrevMoveInputTime <= DodgeDoubleTapTimeout)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("%02f %02f"), LastMoveInputVector.X, LastMoveInputVector.Y));

        FGameplayTagContainer TagContainer;

        TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Player.Ability.Dodge")));

        WarriorAbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer);
    }
}

void AWarriorPlayerCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
    const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();

    if (LookAxisVector.X != 0.f)
    {
        AddControllerYawInput(LookLeftRightRate * LookAxisVector.X * -1.f);
    }

    if (LookAxisVector.Y != 0.f)
    {
        AddControllerPitchInput(LookUpDownRate * LookAxisVector.Y);
    }
}

void AWarriorPlayerCharacter::Input_SwitchTargetTriggered(const FInputActionValue& InputActionValue)
{
    SwitchDirection = InputActionValue.Get<FVector2D>();
}

void AWarriorPlayerCharacter::Input_SwitchTargetCompleted(const FInputActionValue& InputActionValue)
{
    FGameplayEventData Data;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        this,
        SwitchDirection.X > 0.f ? WarriorGameplayTags::Player_Event_SwitchTarget_Left : WarriorGameplayTags::Player_Event_SwitchTarget_Right,
        Data
    );
}

void AWarriorPlayerCharacter::Input_LeftButton(const FInputActionValue& InputActionValue)
{
    FGameplayTagContainer TagContainer;

    TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Player.Ability.Attack")));

    WarriorAbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer);
}

void AWarriorPlayerCharacter::Input_AbilityInputPressed(FGameplayTag _InputTag)
{
    WarriorAbilitySystemComponent->OnAbilityInputPressed(_InputTag);
}

void AWarriorPlayerCharacter::Input_AbilityInputReleased(FGameplayTag _InputTag)
{
    WarriorAbilitySystemComponent->OnAbilityInputReleased(_InputTag);
}
