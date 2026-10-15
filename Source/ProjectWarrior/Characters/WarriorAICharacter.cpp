// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAICharacter.h"
#include "Components/CapsuleComponent.h"
#include "ProjectWarrior/Components/Combat/AICombatComponent.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"
#include "ProjectWarrior/Components/UI/AIUIComponent.h"
#include "Engine/AssetManager.h"
#include "ProjectWarrior/DataAssets/DataAsset_AIStartUpData.h"
#include "Components/WidgetComponent.h"
#include "ProjectWarrior/Widgets/WarriorWidgetBase.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/ProjectWarrior.h"

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

void AWarriorAICharacter::OnCharacterDiedEvent_Implementation()
{
    // 웨이브 처치 카운트 등 사망 알림을 먼저 보냄
    Super::OnCharacterDiedEvent_Implementation();

    // 체력바 숨김. 처형은 연출 시작 시점(GA_AI_Finisher)에 이 함수가 호출되므로 처형 중에도 보이지 않음
    if (AIHealthWidgetComponent)
    {
        AIHealthWidgetComponent->SetVisibility(false, true);
    }

    // 컨트롤러 Focus가 남아 있으면 ALS가 계속 플레이어 쪽을 바라보며 제자리 회전(Turn In Place) 애니메이션을 재생함
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->ClearFocus(EAIFocusPriority::Gameplay);
        AIController->StopMovement();

        if (UBrainComponent* BrainComponent = AIController->GetBrainComponent())
        {
            BrainComponent->StopLogic(TEXT("Dead"));
        }
    }

    // 바라보는 방향 회전·제자리 회전을 끔. 이동 모드는 유지 (사망 몽타주의 루트 모션이 적용되어야 함)
    SetDesiredRotationMode(EALSRotationMode::VelocityDirection);
    SetRotationMode(EALSRotationMode::VelocityDirection);

    GetCharacterMovement()->StopMovementImmediately();
}

void AWarriorAICharacter::BeginPlay()
{
    Super::BeginPlay();

    // 적이 플레이어와 카메라 사이에 들어와도 카메라붐이 당겨지지 않도록 카메라 채널 무시
    // (BP에 저장된 콜리전 프로필이 생성자 설정을 덮어쓰므로 시작 시 적용. 몸 안에 들어간 카메라는 플레이어의 CameraClipHideComponent가 처리)
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    if (UWarriorWidgetBase* HealthWidget = Cast<UWarriorWidgetBase>(AIHealthWidgetComponent->GetUserWidgetObject()))
    {
        HealthWidget->InitAICreatedWidget(this);
    }

    if (WarriorAbilitySystemComponent)
    {
        WarriorAbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(WarriorGameplayTags::AI_Event_Death_PoseReached).AddUObject(this, &ThisClass::OnDeathPoseReached);
    }
}

void AWarriorAICharacter::OnDeathPoseReached(const FGameplayEventData* Payload)
{
    UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(this, WarriorGameplayTags::AI_Status_DeathPoseReached);
}

FText AWarriorAICharacter::GetDisplayName() const
{
    if (!DisplayName.IsEmpty())
    {
        return DisplayName;
    }

    FString ClassName = GetClass()->GetName();
    ClassName.RemoveFromStart(TEXT("BP_"));
    ClassName.RemoveFromEnd(TEXT("_C"));
    return FText::FromString(ClassName);
}

bool AWarriorAICharacter::HasReachedDeathPose() const
{
    return WarriorAbilitySystemComponent && WarriorAbilitySystemComponent->HasMatchingGameplayTag(WarriorGameplayTags::AI_Status_DeathPoseReached);
}

void AWarriorAICharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 재빙의돼도 중복으로 쌓이지 않도록 없는 태그만 추가
    if (WarriorAbilitySystemComponent)
    {
        for (const FGameplayTag& StatusTag : DefaultStatusTags)
        {
            if (!WarriorAbilitySystemComponent->HasMatchingGameplayTag(StatusTag))
            {
                WarriorAbilitySystemComponent->AddLooseGameplayTag(StatusTag);
            }
        }
    }

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
        //시작 데이터가 없는 적도 난이도 배율은 받는다(기본값 1에 곱함)
        bStartUpDataApplied = true;
        ApplyDifficultyScale();
        return;
    }

    // 로드가 끝날 때까지 피해를 받지 않도록 (WarriorAttributeSet::PreGameplayEffectExecute)
    UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(this, WarriorGameplayTags::Shared_Status_Initializing);

    // 로드가 끝나기 전에 적이 사라질 수 있다(스폰 직후 레벨 이동, 즉사). WeakLambda는 this가 무효면 호출하지 않는다
    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        CharacterStartUpData.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(
            this,
            [this]()
            {
                if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.Get())
                {
                    LoadedData->GiveToAbilitySystemComponent(WarriorAbilitySystemComponent);

                    // M9: 시작 GE(Instant Override)가 기본값을 정한 뒤에 난이도 배율을 곱한다. 피해를 받기 시작하기(태그 해제) 전에
                    bStartUpDataApplied = true;
                    ApplyDifficultyScale();

                    UWarriorFunctionLibrary::RemoveGameplayTagFromActorIfFound(this, WarriorGameplayTags::Shared_Status_Initializing);

                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Enemy Start Up Data Loaded")));
                }
            }
        )
    );
}

void AWarriorAICharacter::SetDifficultyScale(float InHealthMultiplier, float InDamageMultiplier)
{
    //서버가 0보다 큰 값만 보내지만, 잘못 와도 체력 0인 적이 생기지 않게 1로 둔다
    DifficultyHealthMultiplier = InHealthMultiplier > 0.f ? InHealthMultiplier : 1.f;
    DifficultyDamageMultiplier = InDamageMultiplier > 0.f ? InDamageMultiplier : 1.f;
    ApplyDifficultyScale();
}

void AWarriorAICharacter::ApplyDifficultyScale()
{
    //시작 데이터가 아직이면 로드 콜백에서 다시 부른다. 한 번만 곱한다
    if (!bStartUpDataApplied || bDifficultyScaleApplied || !WarriorAbilitySystemComponent)
    {
        return;
    }
    bDifficultyScaleApplied = true;

    if (FMath::IsNearlyEqual(DifficultyHealthMultiplier, 1.f) && FMath::IsNearlyEqual(DifficultyDamageMultiplier, 1.f))
    {
        return;
    }

    //최대 체력을 먼저 올린다(현재 체력이 최대를 넘으면 PostAttributeChange가 잘라냄). 체력 비율은 그대로라 보스 페이즈 판정이 바뀌지 않는다
    UAbilitySystemComponent* ASC = WarriorAbilitySystemComponent;
    const float MaxHealth = ASC->GetNumericAttributeBase(UWarriorAttributeSet::GetMaxHealthAttribute());
    const float CurrentHealth = ASC->GetNumericAttributeBase(UWarriorAttributeSet::GetCurrentHealthAttribute());
    const float AttackPower = ASC->GetNumericAttributeBase(UWarriorAttributeSet::GetAttackPowerAttribute());
    ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetMaxHealthAttribute(), MaxHealth * DifficultyHealthMultiplier);
    ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetCurrentHealthAttribute(), CurrentHealth * DifficultyHealthMultiplier);
    ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetAttackPowerAttribute(), AttackPower * DifficultyDamageMultiplier);

    UE_LOG(LogProjectWarrior, Verbose, TEXT("[Stage] %s difficulty scale applied. health x%.2f (%.0f -> %.0f), attack x%.2f"),
        *GetName(), DifficultyHealthMultiplier, MaxHealth, MaxHealth * DifficultyHealthMultiplier, DifficultyDamageMultiplier);
}
