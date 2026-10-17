// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_BladeMode.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/Characters/AnimInstance/WarriorAnimInstance.h"
#include "ProjectWarrior/Controllers/WarriorPlayerController.h"
#include "ProjectWarrior/AbilitySystem/AbilityTask_ExecuteTaskOnTick.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/Audio/WarriorSoundSubsystem.h"
#include "ProjectWarrior/Audio/WarriorSoundTags.h"
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
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
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

	// 전술 모드와 시간 배율을 서로 덮어쓰지 않도록 동시에 켜지 않음
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Player_Status_TacticalMode);

	HitReactEventTag = WarriorGameplayTags::Shared_Event_HitReact_Light;
	WeakPointHitSoundTag = WarriorSoundTags::Sound_Player_BladeMode_WeakPoint;
	MultiWeakPointHitSoundTag = WarriorSoundTags::Sound_Player_BladeMode_WeakPoint_Multi;
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
	BladeBaseFieldOfView = SavedFieldOfView;
	FovPunchRemaining = 0.0f;

	CachedAnimInstance = Cast<UWarriorAnimInstance>(PlayerCharacter->GetMesh()->GetAnimInstance());

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->SetBladeModeState(true, CurrentAimAngle);
	}

	// 사라진 적의 약점 각도는 버림
	for (auto It = WeakLineAngles.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// 인디케이터가 대상 기준 평면을 쓰므로 대상을 먼저 고름
	UpdateWeakLine();
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
	ClearWeakLineTargets();

	// FOV는 아래에서 원래 값으로 되돌림
	FovPunchRemaining = 0.0f;

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

	TickFieldOfView(RealDeltaTime);

	UpdateWeakLine();
	UpdateIndicator();
	TickSlashEffects(RealDeltaTime);
	TickCameraShake(RealDeltaTime);
}

void UPlayerGameplayAbility_BladeMode::TickFieldOfView(float RealDeltaTime)
{
	if (!CachedFollowCamera.IsValid())
	{
		return;
	}

	BladeBaseFieldOfView = FMath::FInterpTo(BladeBaseFieldOfView, ZoomFieldOfView, RealDeltaTime, CameraInterpSpeed);

	// 약점 베기 순간 확 좁혔다가 천천히 돌아옴
	float Punch = 0.0f;

	if (FovPunchRemaining > 0.0f)
	{
		FovPunchRemaining = FMath::Max(0.0f, FovPunchRemaining - RealDeltaTime);
		Punch = FovPunchAmount * FMath::Square(FovPunchRemaining / FovPunchDuration);
	}

	CachedFollowCamera->SetFieldOfView(BladeBaseFieldOfView - Punch);
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
		SavedCameraRelativeLocation + Offset * SlashShakeAmplitude * ShakeScale * Falloff,
		SavedCameraRelativeRotation + FRotator(0.0f, 0.0f, NoiseRoll * SlashShakeRollAmplitude * ShakeScale * Falloff));
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

	if ((!SlashTrailEffect && !SlashSwingParticle) || !PlayerCharacter)
	{
		return;
	}

	// 연달아 벨 때는 이전 궤적을 끝내고 새로 그림(이전 것은 남은 꼬리가 자연스럽게 사라짐)
	StopSlashTrail();

	if (SlashTrailEffect)
	{
		SlashTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(SlashTrailEffect, PlayerCharacter->GetMesh(), SlashTrailSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}

	// 플레이어에 붙으므로 블레이드 모드 슬로모션 중에도 플레이어 속도로 재생됨
	if (SlashSwingParticle)
	{
		SlashSwingParticleComponent = UGameplayStatics::SpawnEmitterAttached(SlashSwingParticle, PlayerCharacter->GetMesh(), SlashSwingParticleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}

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

	if (SlashSwingParticleComponent)
	{
		// 새 입자만 멈추고 이미 나온 불꽃은 수명대로 사라짐(bAutoDestroy로 정리)
		SlashSwingParticleComponent->DeactivateSystem();
		SlashSwingParticleComponent = nullptr;
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

	if (!PlayerCharacter)
	{
		return FVector::ZeroVector;
	}

	// 평면이 대상 쪽으로 돌아 있으면 캐릭터를 지나지 않으므로 평면 위로 옮김
	const FVector Origin = PlayerCharacter->GetActorLocation() + InRotation.GetAxisX() * SlashLineForwardOffset;
	const FVector PlaneNormal = InRotation.GetAxisZ();

	return Origin - PlaneNormal * FVector::DotProduct(Origin - LastSlashPlanePoint, PlaneNormal);
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

void UPlayerGameplayAbility_BladeMode::SpawnSlashMark(const FQuat& InRotation, bool bWeakPointHit, const FVector* InOrigin)
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

	if (bWeakPointHit)
	{
		MarkMaterial->SetVectorParameterValue(TEXT("Color"), WeakLineAlignedColor);
	}

	Mark->SetMaterial(0, MarkMaterial);
	Mark->RegisterComponent();

	const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
	const float SizeX = MeshSize.X > KINDA_SMALL_NUMBER ? MeshSize.X : 100.f;
	const float SizeY = MeshSize.Y > KINDA_SMALL_NUMBER ? MeshSize.Y : 100.f;
	const FQuat StripRotation = FRotationMatrix::MakeFromXY(InRotation.GetAxisY(), InRotation.GetAxisZ()).ToQuat();
	Mark->SetWorldLocationAndRotation(InOrigin ? *InOrigin : GetSlashLineOrigin(InRotation), StripRotation);
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

		// 약점 선에 맞으면 바꿨다가 되돌릴 원래 색
		FLinearColor BaseColor;
		if (IndicatorMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), BaseColor))
		{
			IndicatorBaseColor = BaseColor;
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

	FRotator ViewRotation;

	if (!PlayerCharacter || !GetSlashView(ViewRotation))
	{
		return false;
	}

	// 베는 평면: 앞 방향과 화면상 베는 선을 품는 평면. 캐릭터에서 앞으로 SlashRange만큼
	const FVector Forward = ViewRotation.Vector();
	const FVector LineDirection = GetSlashLineDirection(ViewRotation, InAngle);

	OutRotation = FRotationMatrix::MakeFromXY(Forward, LineDirection).ToQuat();
	OutCenter = PlayerCharacter->GetActorLocation() + Forward * (SlashRange * 0.5f);
	LastSlashPlanePoint = OutCenter;

	return true;
}

bool UPlayerGameplayAbility_BladeMode::GetSlashView(FRotator& OutViewRotation)
{
	const AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	const AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (!PlayerCharacter || !PlayerController)
	{
		return false;
	}

	FVector ViewLocation;
	PlayerController->GetPlayerViewPoint(ViewLocation, OutViewRotation);

	if (const APawn* Target = WeakLineTarget.Get())
	{
		// 카메라의 위쪽은 유지하고 앞쪽만 캐릭터 → 대상 중심으로 돌림(화면 기준 각도가 거의 그대로 통하게)
		const FVector ToTarget = (Target->GetActorLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal();
		const FVector ViewUp = FRotationMatrix(OutViewRotation).GetUnitAxis(EAxis::Z);

		if (!ToTarget.IsNearlyZero())
		{
			OutViewRotation = FRotationMatrix::MakeFromXZ(ToTarget, ViewUp).Rotator();
		}
	}

	return true;
}

bool UPlayerGameplayAbility_BladeMode::CanShowWeakLine(APawn* InPawn)
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	const AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (!InPawn || !PlayerCharacter || !PlayerController || !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InPawn))
	{
		return false;
	}

	// 선이 보이면 반드시 피해가 들어가야 함 → PerformSlash에서 피해를 막는 조건과 똑같이 거름
	if (!UWarriorFunctionLibrary::IsTargetPawnHostile(PlayerCharacter, InPawn)
		|| UWarriorFunctionLibrary::IsActorDead(InPawn)
		|| UWarriorFunctionLibrary::NativeDoesActorHaveTag(InPawn, WarriorGameplayTags::Shared_Status_Dodge)
		|| UWarriorFunctionLibrary::NativeDoesActorHaveTag(InPawn, WarriorGameplayTags::Shared_Status_Invulnerable)
		|| UWarriorFunctionLibrary::NativeDoesActorHaveTag(InPawn, WarriorGameplayTags::Shared_Status_Initializing))
	{
		return false;
	}

	if (UWarriorFunctionLibrary::NativeDoesActorHaveTag(InPawn, WarriorGameplayTags::AI_Status_Guarding)
		&& UWarriorFunctionLibrary::IsValidBlock(PlayerCharacter, InPawn))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TargetCenter = InPawn->GetActorLocation();
	const FVector ToTarget = TargetCenter - ViewLocation;

	// 카메라 정면 원뿔 안
	if (FVector::DotProduct(ToTarget.GetSafeNormal(), ViewRotation.Vector()) < FMath::Cos(FMath::DegreesToRadians(WeakLineTargetConeAngle)))
	{
		return false;
	}

	// 판정 상자 길이 안(평면 축이 캐릭터 → 대상이라 거리만 보면 됨)
	if (FVector::Dist(PlayerCharacter->GetActorLocation(), TargetCenter) > SlashRange)
	{
		return false;
	}

	// 벽 너머의 적은 제외
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(BladeModeWeakLineSight), false, PlayerCharacter);
	TraceParams.AddIgnoredActor(InPawn);

	return !GetWorld()->LineTraceTestByChannel(ViewLocation, TargetCenter, ECC_Visibility, TraceParams);
}

void UPlayerGameplayAbility_BladeMode::FindWeakLineTargets(TArray<APawn*>& OutTargets)
{
	const AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	UWorld* World = GetWorld();

	if (!PlayerCharacter || !World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BladeModeWeakLineTarget), false, PlayerCharacter);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, PlayerCharacter->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(SlashRange), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Candidate = Cast<APawn>(Overlap.GetActor());

		if (Candidate && !OutTargets.Contains(Candidate) && CanShowWeakLine(Candidate))
		{
			OutTargets.Add(Candidate);
		}
	}
}

void UPlayerGameplayAbility_BladeMode::SyncWeakLineTargets(const TArray<APawn*>& InPawns)
{
	// 빠진 적은 원래 오버레이로 되돌림
	for (int32 Index = WeakLineTargets.Num() - 1; Index >= 0; --Index)
	{
		FBladeWeakLineTarget& Entry = WeakLineTargets[Index];

		if (Entry.Pawn.IsValid() && InPawns.Contains(Entry.Pawn.Get()))
		{
			continue;
		}

		for (int32 MeshIndex = 0; MeshIndex < Entry.Meshes.Num(); ++MeshIndex)
		{
			if (UMeshComponent* Mesh = Entry.Meshes[MeshIndex].Get())
			{
				Mesh->SetOverlayMaterial(Entry.SavedOverlayMaterials[MeshIndex]);
			}
		}

		WeakLineTargets.RemoveAt(Index);
	}

	UMaterialInterface* BaseMaterial = nullptr;

	for (APawn* Pawn : InPawns)
	{
		if (WeakLineTargets.ContainsByPredicate([Pawn](const FBladeWeakLineTarget& Entry) { return Entry.Pawn.Get() == Pawn; }))
		{
			continue;
		}

		// ALS 기반 적은 GetMesh()가 숨겨진 애니메이션용이고 보이는 메시가 따로 붙어 있음 → 보이는 스켈레탈 메시 전부에 검
		TArray<USkeletalMeshComponent*> PawnMeshes;
		Pawn->GetComponents<USkeletalMeshComponent>(PawnMeshes);
		PawnMeshes.RemoveAll([](const USkeletalMeshComponent* Mesh) { return !Mesh->IsVisible() || !Mesh->GetSkeletalMeshAsset(); });

		if (PawnMeshes.IsEmpty())
		{
			continue;
		}

		if (!BaseMaterial)
		{
			BaseMaterial = WeakLineMaterial ? WeakLineMaterial.Get()
				: LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MyGameContents/Character/BladeMode/M_BladeWeakLine.M_BladeWeakLine"));

			if (!BaseMaterial)
			{
				return;
			}
		}

		FBladeWeakLineTarget& Entry = WeakLineTargets.AddDefaulted_GetRef();
		Entry.Pawn = Pawn;
		Entry.Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Entry.Material->SetScalarParameterValue(TEXT("Width"), WeakLineWidth);

		for (USkeletalMeshComponent* Mesh : PawnMeshes)
		{
			Entry.SavedOverlayMaterials.Add(Mesh->GetOverlayMaterial());
			Mesh->SetOverlayMaterial(Entry.Material);
			Entry.Meshes.Add(Mesh);
		}
	}
}

void UPlayerGameplayAbility_BladeMode::ClearWeakLineTargets()
{
	SyncWeakLineTargets(TArray<APawn*>());
	WeakLineTarget = nullptr;
}

bool UPlayerGameplayAbility_BladeMode::GetWeakLineRotation(const APawn* InPawn, float InAngle, FQuat& OutRotation)
{
	const AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	const AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (!InPawn || !PlayerCharacter || !PlayerController)
	{
		return false;
	}

	const FVector ToPawn = (InPawn->GetActorLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal();

	if (ToPawn.IsNearlyZero())
	{
		return false;
	}

	// 베는 평면(GetSlashView)과 같은 방식: 카메라 위쪽은 유지하고 앞쪽만 캐릭터 → 적 중심
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FRotator PawnView = FRotationMatrix::MakeFromXZ(ToPawn, FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z)).Rotator();
	OutRotation = FRotationMatrix::MakeFromXY(ToPawn, GetSlashLineDirection(PawnView, InAngle)).ToQuat();

	return true;
}

float UPlayerGameplayAbility_BladeMode::GetWeakLineAngle(const AActor* InActor)
{
	if (const float* Angle = WeakLineAngles.Find(InActor))
	{
		return *Angle;
	}

	const float NewAngle = FMath::FRandRange(-90.0f, 90.0f);
	WeakLineAngles.Add(InActor, NewAngle);

	return NewAngle;
}

void UPlayerGameplayAbility_BladeMode::RerollWeakLineAngle(const AActor* InActor)
{
	// 선은 180도 주기라, 이전 각도에서 [Min, 180 - Min]만큼 돌리면 항상 Min 이상 달라짐
	const float OldAngle = GetWeakLineAngle(InActor);
	const float Offset = FMath::FRandRange(WeakLineMinRerollDelta, 180.0f - WeakLineMinRerollDelta);
	const float NewAngle = FMath::Fmod(OldAngle + Offset + 90.0f, 180.0f) - 90.0f;

	WeakLineAngles.Add(InActor, NewAngle);
}

// 두 선 사이의 각도 차이(0~90). 선은 180도 돌리면 같은 선
static float GetLineAngleDelta(float InA, float InB)
{
	const float Delta = FMath::Abs(FRotator::NormalizeAxis(InA - InB));
	return FMath::Min(Delta, 180.0f - Delta);
}

bool UPlayerGameplayAbility_BladeMode::IsAimAlignedWithWeakLine(const APawn* InPawn)
{
	return InPawn && GetLineAngleDelta(CurrentAimAngle, GetWeakLineAngle(InPawn)) <= WeakLineTolerance;
}

void UPlayerGameplayAbility_BladeMode::UpdateWeakLine()
{
	TArray<APawn*> Targets;

	if (bUseWeakLine)
	{
		FindWeakLineTargets(Targets);
	}

	SyncWeakLineTargets(Targets);

	// 베는 평면 축의 기준: 유효한 동안 유지(축이 매 틱 바뀌어 인디케이터가 튀지 않게), 아니면 화면 중앙에 가장 가까운 적
	if (!WeakLineTarget.IsValid() || !Targets.Contains(WeakLineTarget.Get()))
	{
		WeakLineTarget = nullptr;

		if (const AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo())
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

			float BestDot = -1.0f;

			for (APawn* Candidate : Targets)
			{
				const float Dot = FVector::DotProduct((Candidate->GetActorLocation() - ViewLocation).GetSafeNormal(), ViewRotation.Vector());

				if (Dot > BestDot)
				{
					BestDot = Dot;
					WeakLineTarget = Candidate;
				}
			}
		}
	}

	bool bAnyAligned = false;

	for (const FBladeWeakLineTarget& Entry : WeakLineTargets)
	{
		const APawn* Pawn = Entry.Pawn.Get();
		const float WeakAngle = GetWeakLineAngle(Pawn);
		FQuat WeakRotation;

		if (!Pawn || !Entry.Material || !GetWeakLineRotation(Pawn, WeakAngle, WeakRotation))
		{
			continue;
		}

		const float AngleDelta = GetLineAngleDelta(CurrentAimAngle, WeakAngle);
		const bool bAligned = AngleDelta <= WeakLineTolerance;
		bAnyAligned |= bAligned;

		// 허용 범위 밖에서는 멀수록 흐리게, 안에 들어오면 확 밝게
		const float Closeness = 1.0f - FMath::Clamp((AngleDelta - WeakLineTolerance) / WeakLineFadeRange, 0.0f, 1.0f);
		const float Intensity = bAligned ? WeakLineAlignedIntensity : FMath::Lerp(WeakLineFaintIntensity, WeakLineAlignedIntensity * 0.3f, Closeness);

		// 캐릭터 → 적 중심을 품는 평면. 기준 적은 각도가 같으면 베는 평면과 정확히 겹침
		Entry.Material->SetVectorParameterValue(TEXT("WeakPlanePoint"), FLinearColor(Pawn->GetActorLocation()));
		Entry.Material->SetVectorParameterValue(TEXT("WeakPlaneNormal"), FLinearColor(WeakRotation.GetAxisZ()));
		Entry.Material->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		Entry.Material->SetVectorParameterValue(TEXT("Color"), bAligned ? WeakLineAlignedColor : IndicatorBaseColor);
	}

	if (IndicatorMaterialInstance)
	{
		IndicatorMaterialInstance->SetVectorParameterValue(TEXT("Color"), bAnyAligned ? WeakLineAlignedColor : IndicatorBaseColor);
	}
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

	// 지난 틱 이후 대상이 죽거나 가드했을 수 있으니 대상부터 다시 확인
	UpdateWeakLine();

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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BladeModeSlash), false, PlayerCharacter);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, SlashCenter, SlashRotation, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(SlashExtent), QueryParams);

	TArray<APawn*> HitPawns;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (APawn* OverlappedPawn = Cast<APawn>(Overlap.GetActor()))
		{
			HitPawns.AddUnique(OverlappedPawn);
		}
	}

	// 약점 선이 보이는 적 중 각도가 맞은 적은 판정 상자 밖이어도 전부 맞힘(선이 보이고 각도가 맞으면 반드시 맞는다)
	TArray<APawn*> WeakPointTargets;

	for (const FBladeWeakLineTarget& Entry : WeakLineTargets)
	{
		if (APawn* Pawn = Entry.Pawn.Get(); Pawn && IsAimAlignedWithWeakLine(Pawn))
		{
			WeakPointTargets.Add(Pawn);
			HitPawns.AddUnique(Pawn);
		}
	}

	const float SlashDamage = SlashBaseDamage * GetSlashDamageMultiplier(CurrentAimAngle);
	FGameplayEffectSpecHandle DamageSpecHandle;
	FGameplayEffectSpecHandle WeakPointDamageSpecHandle;

	if (DamageEffectClass)
	{
		DamageSpecHandle = MakePlayerDamageEffectSpecHandle(DamageEffectClass, SlashDamage, WarriorGameplayTags::Player_SetByCaller_AttackType_Light, 1, FGameplayTag());

		if (!WeakPointTargets.IsEmpty())
		{
			WeakPointDamageSpecHandle = MakePlayerDamageEffectSpecHandle(DamageEffectClass, SlashDamage * WeakPointDamageMultiplier,
				WarriorGameplayTags::Player_SetByCaller_AttackType_Light, 1, FGameplayTag());
		}
	}

	int32 WeakPointHitCount = 0;

	for (APawn* HitPawn : HitPawns)
	{
		if (!UWarriorFunctionLibrary::IsTargetPawnHostile(PlayerCharacter, HitPawn))
		{
			continue;
		}

		// 죽은 적은 맞지 않음(피격 모션이 재생되면 시체가 일어섬)
		if (UWarriorFunctionLibrary::IsActorDead(HitPawn))
		{
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

		const bool bIsWeakPointTarget = WeakPointTargets.Contains(HitPawn);
		const FGameplayEffectSpecHandle& SpecToApply = bIsWeakPointTarget ? WeakPointDamageSpecHandle : DamageSpecHandle;

		if (SpecToApply.IsValid() && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitPawn))
		{
			NativeApplyEffectSpecHandleToTarget(HitPawn, SpecToApply);
		}

		if (HitReactEventTag.IsValid())
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(HitPawn, HitReactEventTag, EventData);
		}

		if (bIsWeakPointTarget)
		{
			++WeakPointHitCount;

			// 베는 평면 밖에 있는 적은 그 적의 약점 선 자리에 금색 빛줄기를 따로 그림
			FQuat WeakRotation;

			if (HitPawn != WeakLineTarget.Get() && GetWeakLineRotation(HitPawn, GetWeakLineAngle(HitPawn), WeakRotation))
			{
				const FVector MarkOrigin = HitPawn->GetActorLocation();
				SpawnSlashMark(WeakRotation, true, &MarkOrigin);
			}

			// 약점을 베면 다른 각도의 새 약점이 드러남
			RerollWeakLineAngle(HitPawn);
		}
	}

	SpawnSlashMark(SlashRotation, WeakPointHitCount > 0);
	QueueSlashSparks(SlashRotation);

	// 약점 베기는 일반 베기와 다르게 소리·센 흔들림·순간 줌인을 줌. 동시에 여럿이면 더 세게
	// 소리는 사운드 표(DA_SoundSet)에 태그 칸이 있어야 남
	ShakeScale = 1.0f;

	if (WeakPointHitCount > 0)
	{
		const bool bMultiWeakPointHit = WeakPointHitCount >= 2;
		UWarriorSoundSubsystem::PlaySound2D(PlayerCharacter, bMultiWeakPointHit ? MultiWeakPointHitSoundTag : WeakPointHitSoundTag);

		ShakeScale = bMultiWeakPointHit ? MultiWeakPointShakeScale : WeakPointShakeScale;
		FovPunchAmount = bMultiWeakPointHit ? MultiWeakPointFovPunch : WeakPointFovPunch;
		FovPunchRemaining = FovPunchDuration;

		FGameplayEventData WeakPointEventData;
		WeakPointEventData.Instigator = PlayerCharacter;
		WeakPointEventData.EventMagnitude = WeakPointHitCount;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PlayerCharacter, WarriorGameplayTags::Player_Event_BladeMode_WeakPointHit, WeakPointEventData);
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

	UpdateWeakLine();
	UpdateIndicator();
}
