// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_BladeMode.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "ProjectWarrior/Characters/AnimInstance/WarriorAnimInstance.h"
#include "ProjectWarrior/Controllers/WarriorPlayerController.h"
#include "ProjectWarrior/AbilitySystem/AbilityTask_ExecuteTaskOnTick.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "Misc/App.h"

UPlayerGameplayAbility_BladeMode::UPlayerGameplayAbility_BladeMode()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer BladeModeAssetTags;
	BladeModeAssetTags.AddTag(WarriorGameplayTags::Player_Ability_BladeMode);
	SetAssetTags(BladeModeAssetTags);

	ActivationOwnedTags.AddTag(WarriorGameplayTags::Player_Status_BladeMode);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);

	HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_Light;
}

bool UPlayerGameplayAbility_BladeMode::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (ExhaustedEffect && ASC)
	{
		FGameplayEffectQuery ExhaustedQuery;
		ExhaustedQuery.EffectDefinition = ExhaustedEffect;

		if (ASC->GetActiveEffects(ExhaustedQuery).Num() > 0)
		{
			return false;
		}
	}

	return true;
}

bool UPlayerGameplayAbility_BladeMode::TickStaminaDrain(float RealDeltaTime)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	if (!ASC || !StaminaDrainEffect || StaminaDrainPerSecond <= 0.0f)
	{
		return false;
	}

	StaminaDrainAccumulator += RealDeltaTime;

	while (StaminaDrainAccumulator >= StaminaDrainInterval)
	{
		StaminaDrainAccumulator -= StaminaDrainInterval;

		FGameplayEffectSpecHandle DrainSpec = MakeOutgoingGameplayEffectSpec(StaminaDrainEffect, GetAbilityLevel());

		if (DrainSpec.IsValid())
		{
			DrainSpec.Data->SetSetByCallerMagnitude(WarriorGameplayTags::Player_SetByCaller_StaminaDrain, -StaminaDrainPerSecond * StaminaDrainInterval);
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DrainSpec);
		}
	}

	return ASC->GetNumericAttribute(UWarriorAttributeSet::GetCurrentStaminaAttribute()) <= 0.0f;
}

void UPlayerGameplayAbility_BladeMode::ExhaustBladeMode()
{
	if (ExhaustedEffect)
	{
		ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ExhaustedEffect.GetDefaultObject(), GetAbilityLevel());
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerGameplayAbility_BladeMode::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!GetPlayerCharacterFromActorInfo() || !GetPlayerControllerFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 막기처럼 발동 비용(Cost GE)을 냄. 스태미나가 모자라면 들어가지 않음
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BeginBladeMode();

	UAbilityTask_ExecuteTaskOnTick* TickTask = UAbilityTask_ExecuteTaskOnTick::ExecuteTaskOnTick(this);
	TickTask->OnAbilityTaskTick.AddDynamic(this, &ThisClass::OnBladeModeTick);
	TickTask->ReadyForActivation();
}

void UPlayerGameplayAbility_BladeMode::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RestoreBladeMode();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerGameplayAbility_BladeMode::BeginBladeMode()
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	AimStick = FVector2D(FMath::Cos(FMath::DegreesToRadians(InitialAimAngle)), FMath::Sin(FMath::DegreesToRadians(InitialAimAngle)));
	TargetAimAngle = InitialAimAngle;
	CurrentAimAngle = InitialAimAngle;
	LastSlashRealTime = -1.0;
	StaminaDrainAccumulator = 0.0f;

	// 입력: 블레이드 모드 매핑을 기본·무기 매핑보다 높은 우선순위로 얹어 마우스 이동과 왼쪽 클릭을 가로챔
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (BladeModeMappingContext)
		{
			Subsystem->AddMappingContext(BladeModeMappingContext, BladeModeMappingPriority);
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerCharacter->InputComponent))
	{
		if (AimAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::OnAimInput).GetHandle());
		}

		if (SlashAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(SlashAction, ETriggerEvent::Started, this, &ThisClass::OnSlashInput).GetHandle());
		}

		if (CrossSlashAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(CrossSlashAction, ETriggerEvent::Started, this, &ThisClass::OnCrossSlashInput).GetHandle());
		}
	}

	// 블레이드 모드 중에는 걷지 않음
	PlayerController->SetIgnoreMoveInput(true);

	// 시간: 세상만 느리게, 플레이어는 원래 속도
	SavedPlayerTimeDilation = PlayerCharacter->CustomTimeDilation;
	UGameplayStatics::SetGlobalTimeDilation(PlayerCharacter, WorldTimeDilation);
	PlayerCharacter->CustomTimeDilation = 1.0f / WorldTimeDilation;

	// 카메라
	CachedCameraBoom = PlayerCharacter->FindComponentByClass<USpringArmComponent>();
	CachedFollowCamera = PlayerCharacter->FindComponentByClass<UCameraComponent>();

	if (CachedCameraBoom.IsValid())
	{
		SavedArmLength = CachedCameraBoom->TargetArmLength;
	}

	if (CachedFollowCamera.IsValid())
	{
		SavedFieldOfView = CachedFollowCamera->FieldOfView;
		SavedCameraRelativeLocation = CachedFollowCamera->GetRelativeLocation();
		SavedCameraRelativeRotation = CachedFollowCamera->GetRelativeRotation();
	}

	ShakeRemaining = 0.0f;

	CachedAnimInstance = Cast<UWarriorAnimInstance>(PlayerCharacter->GetMesh()->GetAnimInstance());

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->SetBladeModeState(true, CurrentAimAngle);
	}

	CreateIndicator();

	bBladeModeApplied = true;
}

void UPlayerGameplayAbility_BladeMode::RestoreBladeMode()
{
	if (!bBladeModeApplied)
	{
		return;
	}

	bBladeModeApplied = false;

	StopSlashTrail();
	DestroyIndicator();

	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (PlayerCharacter)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerCharacter->InputComponent))
		{
			for (const uint32 BindingHandle : InputBindingHandles)
			{
				EnhancedInput->RemoveBindingByHandle(BindingHandle);
			}
		}

		UGameplayStatics::SetGlobalTimeDilation(PlayerCharacter, 1.0f);
		PlayerCharacter->CustomTimeDilation = SavedPlayerTimeDilation;
	}

	// 버튼을 떼도 방금 남긴 자국은 끝까지 보여 줌. 아직 안 터진 불꽃은 바로 터뜨림
	HandOffSlashMarks();
	SpawnPendingSparks(true);

	InputBindingHandles.Reset();

	if (PlayerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (BladeModeMappingContext)
			{
				Subsystem->RemoveMappingContext(BladeModeMappingContext);
			}
		}

		PlayerController->ResetIgnoreMoveInput();
	}

	if (CachedCameraBoom.IsValid())
	{
		CachedCameraBoom->TargetArmLength = SavedArmLength;
	}

	if (CachedFollowCamera.IsValid())
	{
		CachedFollowCamera->SetFieldOfView(SavedFieldOfView);
	}

	ResetCameraShake();

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->SetBladeModeState(false, CurrentAimAngle);
	}
}

void UPlayerGameplayAbility_BladeMode::OnBladeModeTick(float DeltaTime)
{
	// 세상이 느려져도 조준·카메라는 실제 시간으로 움직여야 손맛이 남
	const float RealDeltaTime = FApp::GetDeltaTime();

	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (!PlayerCharacter || !PlayerController)
	{
		return;
	}

	// 유지하는 동안 스태미나를 깎고, 바닥나면 디버프를 걸고 끝냄
	if (TickStaminaDrain(RealDeltaTime))
	{
		ExhaustBladeMode();
		return;
	}

	// 가장 가까운 쪽으로 돌아감(170도 → -170도는 20도만 움직임)
	const float DeltaAngle = FMath::FindDeltaAngleDegrees(CurrentAimAngle, TargetAimAngle);
	const float MaxStep = AimRotationSpeed * RealDeltaTime;
	CurrentAimAngle = FRotator::NormalizeAxis(CurrentAimAngle + FMath::Clamp(DeltaAngle, -MaxStep, MaxStep));

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->SetBladeModeState(true, CurrentAimAngle);
	}

	const FRotator ControlRotation = PlayerController->GetControlRotation();
	const FRotator CurrentActorRotation = PlayerCharacter->GetActorRotation();
	const FRotator FaceRotation(0.f, ControlRotation.Yaw, 0.f);
	PlayerCharacter->SetActorRotation(FMath::RInterpTo(CurrentActorRotation, FaceRotation, RealDeltaTime, FaceCameraInterpSpeed));

	if (CachedCameraBoom.IsValid())
	{
		CachedCameraBoom->TargetArmLength = FMath::FInterpTo(CachedCameraBoom->TargetArmLength, ZoomArmLength, RealDeltaTime, CameraInterpSpeed);
	}

	if (CachedFollowCamera.IsValid())
	{
		CachedFollowCamera->SetFieldOfView(FMath::FInterpTo(CachedFollowCamera->FieldOfView, ZoomFieldOfView, RealDeltaTime, CameraInterpSpeed));
	}

	UpdateIndicator();
	TickSlashEffects(RealDeltaTime);
	TickCameraShake(RealDeltaTime);
}

void UPlayerGameplayAbility_BladeMode::TickCameraShake(float RealDeltaTime)
{
	if (ShakeRemaining <= 0.0f || !CachedFollowCamera.IsValid())
	{
		return;
	}

	ShakeRemaining -= RealDeltaTime;
	ShakeTime += RealDeltaTime;

	if (ShakeRemaining <= 0.0f)
	{
		ResetCameraShake();
		return;
	}

	// 처음에 세고 점점 줄어듦. 축마다 다른 위치의 노이즈를 써서 좌우·상하·롤이 따로 흔들리게 함
	const float Falloff = FMath::Square(ShakeRemaining / FMath::Max(SlashShakeDuration, KINDA_SMALL_NUMBER));
	const float Phase = ShakeTime * SlashShakeFrequency;
	const float NoiseRight = FMath::PerlinNoise1D(Phase) * 2.0f;
	const float NoiseUp = FMath::PerlinNoise1D(Phase + 37.1f) * 2.0f;
	const float NoiseRoll = FMath::PerlinNoise1D(Phase + 71.3f) * 2.0f;

	const FVector Offset(0.0f, NoiseRight, NoiseUp);
	CachedFollowCamera->SetRelativeLocationAndRotation(
		SavedCameraRelativeLocation + Offset * SlashShakeAmplitude * Falloff,
		SavedCameraRelativeRotation + FRotator(0.0f, 0.0f, NoiseRoll * SlashShakeRollAmplitude * Falloff));
}

void UPlayerGameplayAbility_BladeMode::ResetCameraShake()
{
	ShakeRemaining = 0.0f;

	if (CachedFollowCamera.IsValid())
	{
		CachedFollowCamera->SetRelativeLocationAndRotation(SavedCameraRelativeLocation, SavedCameraRelativeRotation);
	}
}

void UPlayerGameplayAbility_BladeMode::TickSlashEffects(float RealDeltaTime)
{
	RemoveExpiredSlashMarks();
	SpawnPendingSparks(false);

	if (SlashTrailRemaining > 0.0f)
	{
		SlashTrailRemaining -= RealDeltaTime;

		if (SlashTrailRemaining <= 0.0f)
		{
			StopSlashTrail();
		}
	}

	if (IndicatorFlashRemaining > 0.0f)
	{
		IndicatorFlashRemaining = FMath::Max(0.0f, IndicatorFlashRemaining - RealDeltaTime);

		if (IndicatorMaterialInstance)
		{
			// 번쩍인 뒤 원래 밝기로 줄어듦
			const float FlashRatio = IndicatorFlashTime > 0.0f ? IndicatorFlashRemaining / IndicatorFlashTime : 0.0f;
			IndicatorMaterialInstance->SetScalarParameterValue(TEXT("Intensity"), IndicatorBaseIntensity * FMath::Lerp(1.0f, IndicatorFlashMultiplier, FlashRatio));
		}
	}
}

void UPlayerGameplayAbility_BladeMode::StartSlashTrail()
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();

	if (!SlashTrailEffect || !PlayerCharacter)
	{
		return;
	}

	// 연달아 벨 때는 이전 궤적을 끝내고 새로 그림(이전 것은 남은 꼬리가 자연스럽게 사라짐)
	StopSlashTrail();

	SlashTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(SlashTrailEffect, PlayerCharacter->GetMesh(), SlashTrailSocketName,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	SlashTrailRemaining = SlashTrailDuration;
}

void UPlayerGameplayAbility_BladeMode::StopSlashTrail()
{
	if (SlashTrailComponent)
	{
		// 기본 공격의 TimedNiagaraEffect(bDestroyAtEnd = false)처럼 비활성화만 해서 그려진 궤적은 사라질 때까지 둠
		SlashTrailComponent->Deactivate();
		SlashTrailComponent = nullptr;
	}

	SlashTrailRemaining = 0.0f;
}

UStaticMesh* UPlayerGameplayAbility_BladeMode::GetPlaneMesh() const
{
	return IndicatorMesh ? IndicatorMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
}

FVector UPlayerGameplayAbility_BladeMode::GetSlashLineOrigin(const FQuat& InRotation)
{
	const AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();

	return PlayerCharacter ? PlayerCharacter->GetActorLocation() + InRotation.GetAxisX() * SlashLineForwardOffset : FVector::ZeroVector;
}

void UPlayerGameplayAbility_BladeMode::QueueSlashSparks(const FQuat& InRotation)
{
	if (!SlashSparkEffect || SlashSparkCount <= 0)
	{
		return;
	}

	const FVector Origin = GetSlashLineOrigin(InRotation);
	const FVector LineDirection = InRotation.GetAxisY();
	const FVector PlaneNormal = InRotation.GetAxisZ();

	// 불꽃 이펙트는 X축 원뿔로 튐 → X를 칼이 지나가는 방향(칼이 있던 +Y 쪽에서 반대쪽)으로 맞춰 칼을 따라 튀게 함
	const FQuat SparkRotation = FRotationMatrix::MakeFromXZ(-LineDirection, PlaneNormal).ToQuat();
	const double Now = FPlatformTime::Seconds();

	// 칼이 있던 쪽(+Y) 끝에서 반대쪽 끝으로 빛줄기와 같은 속도로 차례로 터뜨림
	for (int32 Index = 0; Index < SlashSparkCount; ++Index)
	{
		const float Ratio = SlashSparkCount > 1 ? static_cast<float>(Index) / static_cast<float>(SlashSparkCount - 1) : 0.5f;
		const FVector Location = Origin + LineDirection * FMath::Lerp(SlashHalfWidth, -SlashHalfWidth, Ratio);

		PendingSparkTransforms.Add(FTransform(SparkRotation, Location, FVector(SlashSparkScale)));
		PendingSparkTimes.Add(Now + SlashSparkSweepTime * Ratio);
	}

	SpawnPendingSparks(false);
}

void UPlayerGameplayAbility_BladeMode::SpawnPendingSparks(bool bSpawnAll)
{
	UWorld* World = GetWorld();
	const double Now = FPlatformTime::Seconds();

	for (int32 Index = PendingSparkTimes.Num() - 1; Index >= 0; --Index)
	{
		if (!bSpawnAll && Now < PendingSparkTimes[Index])
		{
			continue;
		}

		if (World && SlashSparkEffect)
		{
			const FTransform& SparkTransform = PendingSparkTransforms[Index];
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SlashSparkEffect, SparkTransform.GetLocation(), SparkTransform.Rotator(), SparkTransform.GetScale3D());
		}

		PendingSparkTransforms.RemoveAt(Index);
		PendingSparkTimes.RemoveAt(Index);
	}
}

void UPlayerGameplayAbility_BladeMode::SpawnSlashMark(const FQuat& InRotation)
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	UWorld* World = GetWorld();
	UStaticMesh* Mesh = GetPlaneMesh();

	if (!SlashMarkMaterial || !PlayerCharacter || !World || !Mesh)
	{
		return;
	}

	// 베는 선을 따라 남는 빛줄기. 베는 평면은 카메라 시선을 품어 옆면으로 보이므로,
	// 선 방향(X)과 평면 법선(Y)으로 띠를 만들어 카메라를 향하게 함. 인디케이터가 움직여도 벤 자리에 그대로 둠
	UStaticMeshComponent* Mark = NewObject<UStaticMeshComponent>(PlayerCharacter, NAME_None, RF_Transient);
	Mark->SetStaticMesh(Mesh);
	Mark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mark->SetCastShadow(false);
	Mark->SetUsingAbsoluteLocation(true);
	Mark->SetUsingAbsoluteRotation(true);
	Mark->SetUsingAbsoluteScale(true);
	Mark->SetupAttachment(PlayerCharacter->GetRootComponent());

	// 머티리얼이 이 시각부터 실제 시간으로 빛을 쓸고(띠의 +X = 칼이 있던 쪽 → -X) 사라지게 함
	UMaterialInstanceDynamic* MarkMaterial = UMaterialInstanceDynamic::Create(SlashMarkMaterial, Mark);
	MarkMaterial->SetScalarParameterValue(TEXT("StartRealTime"), World->GetRealTimeSeconds());
	Mark->SetMaterial(0, MarkMaterial);
	Mark->RegisterComponent();

	const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
	const float SizeX = MeshSize.X > KINDA_SMALL_NUMBER ? MeshSize.X : 100.f;
	const float SizeY = MeshSize.Y > KINDA_SMALL_NUMBER ? MeshSize.Y : 100.f;
	const FQuat StripRotation = FRotationMatrix::MakeFromXY(InRotation.GetAxisY(), InRotation.GetAxisZ()).ToQuat();
	Mark->SetWorldLocationAndRotation(GetSlashLineOrigin(InRotation), StripRotation);
	Mark->SetWorldScale3D(FVector(SlashHalfWidth * 2.f / SizeX, SlashLineWidth / SizeY, 1.f));

	SlashMarks.Add(Mark);
	SlashMarkExpireTimes.Add(FPlatformTime::Seconds() + SlashMarkLifetime);
}

void UPlayerGameplayAbility_BladeMode::RemoveExpiredSlashMarks()
{
	const double Now = FPlatformTime::Seconds();

	for (int32 Index = SlashMarks.Num() - 1; Index >= 0; --Index)
	{
		if (Now < SlashMarkExpireTimes[Index])
		{
			continue;
		}

		if (SlashMarks[Index])
		{
			SlashMarks[Index]->DestroyComponent();
		}

		SlashMarks.RemoveAt(Index);
		SlashMarkExpireTimes.RemoveAt(Index);
	}
}

void UPlayerGameplayAbility_BladeMode::HandOffSlashMarks()
{
	UWorld* World = GetWorld();
	const double Now = FPlatformTime::Seconds();

	for (int32 Index = 0; Index < SlashMarks.Num(); ++Index)
	{
		TWeakObjectPtr<UStaticMeshComponent> WeakMark = SlashMarks[Index];

		if (!WeakMark.IsValid())
		{
			continue;
		}

		// 시간 배율을 되돌린 뒤라 타이머 시간 = 실제 시간
		const float Remaining = FMath::Max(0.01f, static_cast<float>(SlashMarkExpireTimes[Index] - Now));

		if (World)
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakMark]()
			{
				if (WeakMark.IsValid())
				{
					WeakMark->DestroyComponent();
				}
			}), Remaining, false);
		}
		else
		{
			WeakMark->DestroyComponent();
		}
	}

	SlashMarks.Reset();
	SlashMarkExpireTimes.Reset();
}

void UPlayerGameplayAbility_BladeMode::CreateIndicator()
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();

	if (!bShowIndicator || !PlayerCharacter)
	{
		return;
	}

	UStaticMesh* Mesh = GetPlaneMesh();

	if (!Mesh)
	{
		return;
	}

	// 판정과 같은 월드 위치로 매 틱 옮기므로 캐릭터에 붙이지 않고 월드 기준으로 둠
	IndicatorComponent = NewObject<UStaticMeshComponent>(PlayerCharacter, NAME_None, RF_Transient);
	IndicatorComponent->SetStaticMesh(Mesh);
	IndicatorComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndicatorComponent->SetCastShadow(false);
	IndicatorComponent->SetUsingAbsoluteLocation(true);
	IndicatorComponent->SetUsingAbsoluteRotation(true);
	IndicatorComponent->SetUsingAbsoluteScale(true);

	if (IndicatorMaterial)
	{
		// 벨 때 번쩍이게 Intensity를 바꿀 수 있도록 동적 인스턴스로 씀
		IndicatorMaterialInstance = UMaterialInstanceDynamic::Create(IndicatorMaterial, IndicatorComponent);
		IndicatorComponent->SetMaterial(0, IndicatorMaterialInstance);

		float BaseIntensity = 1.0f;
		if (IndicatorMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Intensity")), BaseIntensity))
		{
			IndicatorBaseIntensity = BaseIntensity;
		}
	}

	IndicatorComponent->SetupAttachment(PlayerCharacter->GetRootComponent());
	IndicatorComponent->RegisterComponent();

	// 메시 크기(기본 평면 100x100)에 맞춰 판정 상자 크기로 늘림
	const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
	const float SizeX = MeshSize.X > KINDA_SMALL_NUMBER ? MeshSize.X : 100.f;
	const float SizeY = MeshSize.Y > KINDA_SMALL_NUMBER ? MeshSize.Y : 100.f;
	IndicatorComponent->SetWorldScale3D(FVector(SlashRange / SizeX, SlashHalfWidth * 2.f / SizeY, 1.f));

	UpdateIndicator();
}

void UPlayerGameplayAbility_BladeMode::UpdateIndicator()
{
	if (!IndicatorComponent)
	{
		return;
	}

	FVector Center;
	FQuat Rotation;

	if (GetSlashTransform(CurrentAimAngle, Center, Rotation))
	{
		// 판정 상자는 X·Y가 평면, Z가 두께. 평면 메시도 XY 평면이므로 같은 회전을 그대로 씀
		IndicatorComponent->SetWorldLocationAndRotation(Center, Rotation);
	}
}

void UPlayerGameplayAbility_BladeMode::DestroyIndicator()
{
	if (IndicatorComponent)
	{
		IndicatorComponent->DestroyComponent();
		IndicatorComponent = nullptr;
	}

	IndicatorMaterialInstance = nullptr;
	IndicatorFlashRemaining = 0.0f;
}

bool UPlayerGameplayAbility_BladeMode::GetSlashTransform(float InAngle, FVector& OutCenter, FQuat& OutRotation)
{
	const AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	const AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (!PlayerCharacter || !PlayerController)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 베는 평면: 카메라 앞 방향과 화면상 베는 선을 품는 평면
	const FVector Forward = ViewRotation.Vector();
	const FVector LineDirection = GetSlashLineDirection(ViewRotation, InAngle);

	OutRotation = FRotationMatrix::MakeFromXY(Forward, LineDirection).ToQuat();
	OutCenter = PlayerCharacter->GetActorLocation() + Forward * (SlashRange * 0.5f);

	return true;
}

void UPlayerGameplayAbility_BladeMode::OnAimInput(const FInputActionValue& InputActionValue)
{
	FVector2D MouseDelta = InputActionValue.Get<FVector2D>();

	if (bInvertAimY)
	{
		MouseDelta.Y *= -1.f;
	}

	// 마우스 이동을 쌓아 가상 스틱을 만들고, 길이는 1로 제한(반대로 밀면 금방 방향이 바뀜)
	AimStick += MouseDelta * AimSensitivity;

	if (AimStick.SizeSquared() > 1.f)
	{
		AimStick.Normalize();
	}

	if (AimStick.Size() >= AimDeadZone)
	{
		TargetAimAngle = FMath::RadiansToDegrees(FMath::Atan2(AimStick.Y, AimStick.X));
	}
}

bool UPlayerGameplayAbility_BladeMode::ConsumeSlashInterval()
{
	const double Now = FPlatformTime::Seconds();

	if (LastSlashRealTime >= 0.0 && Now - LastSlashRealTime < SlashInterval)
	{
		return false;
	}

	LastSlashRealTime = Now;
	return true;
}

void UPlayerGameplayAbility_BladeMode::SnapAimAngle(float InAngle)
{
	const float AngleRad = FMath::DegreesToRadians(InAngle);
	const float StickLength = FMath::Max(AimStick.Size(), AimDeadZone);

	AimStick = FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * StickLength;
	CurrentAimAngle = FRotator::NormalizeAxis(InAngle);
	TargetAimAngle = CurrentAimAngle;
}

void UPlayerGameplayAbility_BladeMode::OnSlashInput(const FInputActionValue& InputActionValue)
{
	if (ConsumeSlashInterval())
	{
		PerformSlash();
	}
}

void UPlayerGameplayAbility_BladeMode::OnCrossSlashInput(const FInputActionValue& InputActionValue)
{
	if (!ConsumeSlashInterval())
	{
		return;
	}

	// 칼 자세를 엇갈린 각도로 바로 옮기고 그 각도로 벰
	SnapAimAngle(CurrentAimAngle + CrossSlashAngleOffset);

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->SetBladeModeState(true, CurrentAimAngle);
	}

	UpdateIndicator();
	PerformSlash();
}

FVector UPlayerGameplayAbility_BladeMode::GetSlashLineDirection(const FRotator& ViewRotation, float InAngle) const
{
	const FRotationMatrix ViewMatrix(ViewRotation);
	const float AngleRad = FMath::DegreesToRadians(InAngle);

	return (ViewMatrix.GetUnitAxis(EAxis::Y) * FMath::Cos(AngleRad) + ViewMatrix.GetUnitAxis(EAxis::Z) * FMath::Sin(AngleRad)).GetSafeNormal();
}

void UPlayerGameplayAbility_BladeMode::ReplayDeathFall(APawn* InDeadPawn) const
{
	const AWarriorAICharacter* DeadAI = Cast<AWarriorAICharacter>(InDeadPawn);

	if (!DeadAI || DeadAI->HasReachedDeathPose())
	{
		return;
	}

	UAnimInstance* AnimInstance = DeadAI->GetMesh() ? DeadAI->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* DeathMontage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;

	// 새 몽타주를 틀면 사망 GA의 PlayMontageAndWait가 끊기므로 같은 몽타주를 처음으로 되감기만 함(PoseReached는 다시 기다림)
	if (DeathMontage)
	{
		AnimInstance->Montage_SetPosition(DeathMontage, 0.0f);
	}
}

float UPlayerGameplayAbility_BladeMode::GetSlashDamageMultiplier(float InAngle) const
{
	// 가로(0·180도)에서 얼마나 벗어났는지 0~90도 → 0~1
	const float AbsAngle = FMath::Abs(FRotator::NormalizeAxis(InAngle));
	const float FromHorizontal = FMath::Min(AbsAngle, 180.0f - AbsAngle);
	const float Verticalness = FMath::Clamp(FromHorizontal / 90.0f, 0.0f, 1.0f);

	// 곱셈 기준 보간: 0.5배 ~ 2배일 때 45도에서 정확히 1배
	return HorizontalSlashDamageMultiplier * FMath::Pow(VerticalSlashDamageMultiplier / HorizontalSlashDamageMultiplier, Verticalness);
}

void UPlayerGameplayAbility_BladeMode::PerformSlash()
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();
	UWorld* World = GetWorld();

	if (!PlayerCharacter || !PlayerController || !World)
	{
		return;
	}

	// 칼이 지금 겨눈 각도 그대로 벤다(인디케이터와 같은 평면)
	FVector SlashCenter;
	FQuat SlashRotation;

	if (!GetSlashTransform(CurrentAimAngle, SlashCenter, SlashRotation))
	{
		return;
	}

	const FVector SlashExtent(SlashRange * 0.5f, SlashHalfWidth, SlashHalfThickness);

	if (bDrawDebugSlash)
	{
		DrawDebugBox(World, SlashCenter, SlashExtent, SlashRotation, FColor::Cyan, false, 0.5f, 0, 1.5f);
	}

	if (SlashEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SlashEffect, SlashCenter, SlashRotation.Rotator());
	}

	SpawnSlashMark(SlashRotation);
	QueueSlashSparks(SlashRotation);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BladeModeSlash), false, PlayerCharacter);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, SlashCenter, SlashRotation, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(SlashExtent), QueryParams);

	FGameplayEffectSpecHandle DamageSpecHandle;

	if (DamageEffectClass)
	{
		const float SlashDamage = SlashBaseDamage * GetSlashDamageMultiplier(CurrentAimAngle);
		DamageSpecHandle = MakePlayerDamageEffectSpecHandle(DamageEffectClass, SlashDamage, WarriorGameplayTags::Player_SetByCaller_AttackType_Light, 1, FGameplayTag());
	}

	TSet<AActor*> ProcessedActors;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* HitPawn = Cast<APawn>(Overlap.GetActor());

		if (!HitPawn || ProcessedActors.Contains(HitPawn) || !UWarriorFunctionLibrary::IsTargetPawnHostile(PlayerCharacter, HitPawn))
		{
			continue;
		}

		ProcessedActors.Add(HitPawn);

		// 죽은 적은 데미지·피격 반응을 주지 않음(피격 모션이 재생되면 시체가 일어섬).
		// 아직 쓰러지는 중이면 사망 모션을 처음부터 다시 보여 주고, 다 쓰러진 뒤에는 맞지 않음
		if (UWarriorFunctionLibrary::IsActorDead(HitPawn))
		{
			ReplayDeathFall(HitPawn);
			continue;
		}

		// 회피 중이거나 무적인 대상은 피해 없음
		if (UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitPawn, WarriorGameplayTags::Shared_Status_Dodge)
			|| UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitPawn, WarriorGameplayTags::Shared_Status_Invulnerable))
		{
			continue;
		}

		FGameplayEventData EventData;
		EventData.Instigator = PlayerCharacter;
		EventData.Target = HitPawn;

		// 정면 가드는 막힘 (무기 판정과 같은 규칙)
		if (UWarriorFunctionLibrary::NativeDoesActorHaveTag(HitPawn, WarriorGameplayTags::AI_Status_Guarding)
			&& UWarriorFunctionLibrary::IsValidBlock(PlayerCharacter, HitPawn))
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, WarriorGameplayTags::AI_Event_GuardHit, EventData);
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PlayerCharacter, WarriorGameplayTags::Player_Event_AttackBlocked, EventData);
			continue;
		}

		if (DamageSpecHandle.IsValid() && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitPawn))
		{
			NativeApplyEffectSpecHandleToTarget(HitPawn, DamageSpecHandle);
		}

		if (HitReactEventTag.IsValid())
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, HitReactEventTag, EventData);
		}
	}

	// 벤 뒤에는 칼이 반대편에 있음. 다음 베기는 그 자리에서 시작
	// 각도를 돌려 보간하면 칼이 원을 그리며 돌아가므로, 벤 자세에서 반대편 자세로 바로 섞어 몸 앞을 가로지르게 함
	const float SlashedAngle = CurrentAimAngle;
	AimStick = -AimStick;
	CurrentAimAngle = FRotator::NormalizeAxis(CurrentAimAngle + 180.f);
	TargetAimAngle = FRotator::NormalizeAxis(TargetAimAngle + 180.f);

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->SetBladeModeState(true, CurrentAimAngle);
		CachedAnimInstance->StartBladeSlashBlend(SlashedAngle, SlashPoseBlendTime);
	}

	// 칼이 몸 앞을 가로지르는 동안 궤적을 그림
	StartSlashTrail();
	IndicatorFlashRemaining = IndicatorFlashTime;

	if (SlashShakeDuration > 0.0f)
	{
		ShakeRemaining = SlashShakeDuration;
		ShakeTime = FMath::FRand() * 100.0f;
	}

	UpdateIndicator();
}
