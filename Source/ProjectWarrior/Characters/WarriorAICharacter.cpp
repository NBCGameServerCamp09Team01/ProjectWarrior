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
#include "GameplayEffect.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

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

    // 쓰러진 몸에는 수식어 오라·외곽선을 남기지 않음
    if (AffixAuraComponent)
    {
        AffixAuraComponent->Deactivate();
    }
    if (AffixOverlayMaterial)
    {
        TArray<USkeletalMeshComponent*> Meshes;
        GetComponents<USkeletalMeshComponent>(Meshes);
        for (USkeletalMeshComponent* MeshComponent : Meshes)
        {
            if (MeshComponent && MeshComponent->GetOverlayMaterial() == AffixOverlayMaterial)
            {
                MeshComponent->SetOverlayMaterial(nullptr);
            }
        }
        AffixOverlayMaterial = nullptr;
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

    // 쓰러진 몸(캡슐·메시)이 플레이어 이동을 막지 않도록 폰 채널 무시. 바닥(WorldStatic) 충돌은 유지
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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
        //시작 데이터가 없는 적도 난이도 배율은 받는다(기본값 1에 곱함). 빙의는 SpawnActor 안이라 배율은 이 뒤에 온다
        if (DifficultyScaleGate.MarkStartUpDataApplied())
        {
            ApplyDifficultyScale();
        }
        TryApplyAffixes();
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
                    if (DifficultyScaleGate.MarkStartUpDataApplied())
                    {
                        ApplyDifficultyScale();
                    }
                    // 수식어는 난이도 배율 위에 곱한다(무한 GE)
                    TryApplyAffixes();

                    UWarriorFunctionLibrary::RemoveGameplayTagFromActorIfFound(this, WarriorGameplayTags::Shared_Status_Initializing);

                    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Enemy Start Up Data Loaded")));
                }
            }
        )
    );
}

void AWarriorAICharacter::SetDifficultyScale(float InHealthMultiplier, float InDamageMultiplier)
{
    //시작 데이터가 아직이면 로드 콜백에서 곱한다. 0 이하·한 번만·배율 1은 판정(FWarriorDifficultyScaleGate)이 맡는다
    if (DifficultyScaleGate.SetScale(InHealthMultiplier, InDamageMultiplier))
    {
        ApplyDifficultyScale();
    }
}

void AWarriorAICharacter::ApplyDifficultyScale()
{
    if (!WarriorAbilitySystemComponent)
    {
        return;
    }
    const float DifficultyHealthMultiplier = DifficultyScaleGate.HealthMultiplier;
    const float DifficultyDamageMultiplier = DifficultyScaleGate.DamageMultiplier;

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

void AWarriorAICharacter::SetAffixes(const TArray<FName>& InAffixIds)
{
    if (InAffixIds.IsEmpty() || bAffixesApplied)
    {
        return;
    }

    const UWarriorAffixCatalogDataAsset* Catalog = UWarriorAffixCatalogDataAsset::LoadFromSettings();
    if (!Catalog)
    {
        UE_LOG(LogProjectWarrior, Warning, TEXT("[Affix] %s: AffixCatalog is not set in Project Settings > Warrior Stage. %d affix(es) ignored."),
            *GetName(), InAffixIds.Num());
        return;
    }

    // 모르는 키는 그 수식어만 뺀다(판은 그대로, stage-api.md "수식어" 제안)
    AffixIds.Reset();
    for (const FName AffixId : InAffixIds)
    {
        if (Catalog->FindAffix(AffixId))
        {
            AffixIds.AddUnique(AffixId);
        }
        else
        {
            UE_LOG(LogProjectWarrior, Warning, TEXT("[Affix] %s: unknown affix '%s' ignored (not in DA_AffixCatalog)."),
                *GetName(), *AffixId.ToString());
        }
    }

    TryApplyAffixes();
}

void AWarriorAICharacter::ApplyAffixOutline(const UWarriorAffixCatalogDataAsset& InCatalog)
{
    UMaterialInterface* OutlineMaterial = InCatalog.GetOutlineMaterial();
    if (!OutlineMaterial)
    {
        return;
    }

    UMaterialInstanceDynamic* OutlineInstance = UMaterialInstanceDynamic::Create(OutlineMaterial, this);
    OutlineInstance->SetVectorParameterValue(TEXT("TierColor"), InCatalog.GetTierColor(GetAffixTier()));
    AffixOverlayMaterial = OutlineInstance;

    // ALS 기반 적은 GetMesh()가 숨겨진 애니메이션용이고 보이는 메시가 따로 붙어 있다(블레이드 모드 약점선과 같은 고르기)
    TArray<USkeletalMeshComponent*> Meshes;
    GetComponents<USkeletalMeshComponent>(Meshes);
    for (USkeletalMeshComponent* MeshComponent : Meshes)
    {
        // 이미 다른 오버레이(블레이드 모드 약점선)가 있으면 그쪽이 끝날 때 GetAffixOverlayMaterial로 되돌린다
        if (MeshComponent && MeshComponent->IsVisible() && MeshComponent->GetSkeletalMeshAsset() && !MeshComponent->GetOverlayMaterial())
        {
            MeshComponent->SetOverlayMaterial(OutlineInstance);
        }
    }
}

void AWarriorAICharacter::SpawnAffixAura(const UWarriorAffixCatalogDataAsset& InCatalog)
{
    UNiagaraSystem* AuraSystem = InCatalog.GetAuraSystem();
    const FWarriorAffixDefinition* FirstAffix = AffixIds.IsEmpty() ? nullptr : InCatalog.FindAffix(AffixIds[0]);
    if (!AuraSystem || !FirstAffix)
    {
        return;
    }

    // 오라는 메시를 붙은 부모에서 찾으므로 보이는 스켈레탈 메시에 붙인다(GetMesh()는 숨겨진 애니메이션용일 수 있음)
    TArray<USkeletalMeshComponent*> Meshes;
    GetComponents<USkeletalMeshComponent>(Meshes);
    USkeletalMeshComponent* const* AuraParent = Meshes.FindByPredicate([](const USkeletalMeshComponent* MeshComponent)
    {
        return MeshComponent && MeshComponent->IsVisible() && MeshComponent->GetSkeletalMeshAsset();
    });
    if (!AuraParent)
    {
        return;
    }

    AffixAuraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(AuraSystem, *AuraParent, NAME_None, FVector::ZeroVector,
        FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
    if (AffixAuraComponent)
    {
        AffixAuraComponent->SetVariableLinearColor(TEXT("User.AuraColor"), FirstAffix->AuraColor * InCatalog.GetAuraIntensity());
    }
}

void AWarriorAICharacter::TryApplyAffixes()
{
    if (bAffixesApplied || AffixIds.IsEmpty() || !DifficultyScaleGate.bStartUpDataApplied)
    {
        return;
    }
    bAffixesApplied = true;
    ApplyAffixes();
}

void AWarriorAICharacter::ApplyAffixes()
{
    const UWarriorAffixCatalogDataAsset* Catalog = UWarriorAffixCatalogDataAsset::LoadFromSettings();
    if (!Catalog || !WarriorAbilitySystemComponent)
    {
        return;
    }

    UAbilitySystemComponent* ASC = WarriorAbilitySystemComponent;
    const float OldMaxHealth = ASC->GetNumericAttribute(UWarriorAttributeSet::GetMaxHealthAttribute());

    for (const FName AffixId : AffixIds)
    {
        const FWarriorAffixDefinition* Definition = Catalog->FindAffix(AffixId);
        if (!Definition)
        {
            continue;
        }

        if (Definition->AffixTag.IsValid())
        {
            UWarriorFunctionLibrary::AddGameplayTagToActorIfNone(this, Definition->AffixTag);
        }

        for (const TSubclassOf<UGameplayEffect>& EffectClass : Definition->SelfEffects)
        {
            if (EffectClass)
            {
                ASC->ApplyGameplayEffectToSelf(EffectClass.GetDefaultObject(), 1.f, ASC->MakeEffectContext());
            }
        }

        // 신속: 최종 최고 속도에 배율(MaxWalkSpeed 직접 변경은 ALS가 걸음이 바뀔 때 덮어씀)
        if (!FMath::IsNearlyEqual(Definition->SelfMoveSpeedScale, 1.f))
        {
            SetMovementSpeedScale(AffixId, Definition->SelfMoveSpeedScale);
        }
    }

    // 강인처럼 최대 체력을 올리는 수식어: 무한 GE는 최대만 바꾸므로 현재 체력을 같은 비율로 올린다(가득 찬 채 시작)
    const float NewMaxHealth = ASC->GetNumericAttribute(UWarriorAttributeSet::GetMaxHealthAttribute());
    if (OldMaxHealth > 0.f && !FMath::IsNearlyEqual(NewMaxHealth, OldMaxHealth))
    {
        const float CurrentHealth = ASC->GetNumericAttributeBase(UWarriorAttributeSet::GetCurrentHealthAttribute());
        ASC->SetNumericAttributeBase(UWarriorAttributeSet::GetCurrentHealthAttribute(), CurrentHealth * NewMaxHealth / OldMaxHealth);
    }

    // 체력바 수식어 줄(예: "강인 · 강력")과 등급 색
    TArray<FString> Names;
    TArray<FString> DisplayNames;
    for (const FName AffixId : AffixIds)
    {
        Names.Add(AffixId.ToString());
        if (const FWarriorAffixDefinition* Definition = Catalog->FindAffix(AffixId))
        {
            DisplayNames.Add(Definition->DisplayName.IsEmpty() ? AffixId.ToString() : Definition->DisplayName.ToString());
        }
    }
    if (AIUIComponent)
    {
        AIUIComponent->SetAffixDisplay(FText::FromString(FString::Join(DisplayNames, TEXT(" · "))), Catalog->GetTierColor(GetAffixTier()));
    }
    ApplyAffixOutline(*Catalog);
    SpawnAffixAura(*Catalog);
    UE_LOG(LogProjectWarrior, Log, TEXT("[Affix] %s applied [%s] tier=%s maxHealth %.0f -> %.0f"),
        *GetName(), *FString::Join(Names, TEXT(", ")), *UEnum::GetValueAsString(GetAffixTier()), OldMaxHealth, NewMaxHealth);
}
