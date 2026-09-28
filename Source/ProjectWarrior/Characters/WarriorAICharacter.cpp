// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAICharacter.h"
#include "ProjectWarrior/Components/Combat/AICombatComponent.h"
#include "ProjectWarrior/Components/UI/AIUIComponent.h"
#include "Engine/AssetManager.h"
#include "ProjectWarrior/DataAssets/DataAsset_AIStartUpData.h"
#include "Components/WidgetComponent.h"
#include "ProjectWarrior/Widgets/WarriorWidgetBase.h"

AWarriorAICharacter::AWarriorAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AICombatComponent = CreateDefaultSubobject<UAICombatComponent>("AICombatComponent");

    AIUIComponent = CreateDefaultSubobject<UAIUIComponent>("AIUIComponent");

    AIHealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>("AIHealthWidgetComponent");
    AIHealthWidgetComponent->SetupAttachment(GetMesh());
}

UPawnCombatComponent* AWarriorAICharacter::GetPawnCombatComponent() const
{
    return AICombatComponent;
}

UPawnUIComponent* AWarriorAICharacter::GetPawnUIComponent() const
{
    return AIUIComponent;
}

UAIUIComponent* AWarriorAICharacter::GetAIUIComponent() const
{
    return AIUIComponent;
}

void AWarriorAICharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UWarriorWidgetBase* HealthWidget = Cast<UWarriorWidgetBase>(AIHealthWidgetComponent->GetUserWidgetObject()))
    {
        HealthWidget->InitAICreatedWidget(this);
    }
}

void AWarriorAICharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    //if (!CharacterStartUpData.IsNull())
    //{
    //    if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous())
    //    {
    //        LoadedData->GiveToAbilitySystemComponent(WarriorAbilitySystemComponent);
    //    }
    //}

    InitAIStartUpData();
}

void AWarriorAICharacter::InitAIStartUpData()
{
    if (CharacterStartUpData.IsNull())
    {
        return;
    }

    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        CharacterStartUpData.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda(
            [this]()
            {
                if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.Get())
                {
                    LoadedData->GiveToAbilitySystemComponent(WarriorAbilitySystemComponent);

                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Enemy Start Up Data Loaded")));
                }
            }
        )
    );
}
