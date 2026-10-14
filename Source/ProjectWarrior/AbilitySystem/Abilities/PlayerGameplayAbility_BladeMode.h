// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayerGameplayAbility.h"
#include "PlayerGameplayAbility_BladeMode.generated.h"

class UInputMappingContext;
class UInputAction;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UNiagaraComponent;
class USpringArmComponent;
class UCameraComponent;
class UWarriorAnimInstance;
struct FInputActionValue;

// 약점 선을 그리고 있는 적 하나. 보이는 메시마다 원래 오버레이를 기억했다가 되돌림
USTRUCT()
struct FBladeWeakLineTarget
{
	GENERATED_BODY()

	TWeakObjectPtr<APawn> Pawn;

	TArray<TWeakObjectPtr<UMeshComponent>> Meshes;

	// Meshes와 같은 순서
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SavedOverlayMaterials;

	// 적마다 평면이 달라서 머티리얼도 따로 씀
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
};

/**
 * 블레이드 모드(메탈기어 라이징). 버튼을 누르고 있는 동안 세상이 느려지고, 마우스로 베는 각도를 정해 클릭하면 그 평면으로 벤다.
 * 칼을 겨눈 자세는 애님 그래프의 상체 레이어(BS_BladeAim)가 각도에 맞춰 섞는다.
 */
UCLASS()
class PROJECTWARRIOR_API UPlayerGameplayAbility_BladeMode : public UPlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerGameplayAbility_BladeMode();

protected:
	// 탈진 디버프가 남아 있으면 들어갈 수 없음
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnBladeModeTick(float DeltaTime);

	void OnAimInput(const FInputActionValue& InputActionValue);
	void OnSlashInput(const FInputActionValue& InputActionValue);
	void OnCrossSlashInput(const FInputActionValue& InputActionValue);

	// 연속 클릭 간격을 지켰으면 true
	bool ConsumeSlashInterval();

	// 조준을 즉시 InAngle로 옮김(보간 없이)
	void SnapAimAngle(float InAngle);

	void PerformSlash();

	// 베는 각도(0 = 가로, ±90 = 세로)에 따른 데미지 배율
	float GetSlashDamageMultiplier(float InAngle) const;

	// 화면 기준 베는 선의 방향(월드). 0도 = 카메라 오른쪽, 90도 = 카메라 위
	FVector GetSlashLineDirection(const FRotator& ViewRotation, float InAngle) const;

	// 베는 판정 상자의 중심과 회전(X = 카메라 앞, Y = 베는 선). 인디케이터도 같은 값을 씀.
	// 약점 선 대상이 있으면 평면이 캐릭터 → 대상 중심을 지나게 돌려, 어떤 각도로 베어도 대상을 지나감
	bool GetSlashTransform(float InAngle, FVector& OutCenter, FQuat& OutRotation);

	// 베는 평면의 회전 기준(위쪽은 카메라와 같음). 약점 선 대상이 있으면 X축이 캐릭터 → 대상 중심.
	// 카메라를 지나는 평면은 옆면으로만 보여 인디케이터가 사라지므로 축은 캐릭터에서 시작
	bool GetSlashView(FRotator& OutViewRotation);

#pragma region WeakLineLogic
	// 범위 안에서 약점 선을 보여도 되는 적 전부
	void FindWeakLineTargets(TArray<APawn*>& OutTargets);

	// 약점 선을 보여도 되는지(= 베면 반드시 피해가 들어가는 상태인지)
	bool CanShowWeakLine(APawn* InPawn);

	// 오버레이를 InPawns에만 걸리게 맞춤(빠진 적은 되돌리고 새 적은 검)
	void SyncWeakLineTargets(const TArray<APawn*>& InPawns);
	void ClearWeakLineTargets();
	void UpdateWeakLine();

	// 적의 약점 평면 회전(X = 캐릭터 → 적 중심, Y = 약점 선, Z = 평면 법선)
	bool GetWeakLineRotation(const APawn* InPawn, float InAngle, FQuat& OutRotation);

	// 대상의 약점 각도(-90~90, 선이라 180도 주기). 처음 보는 적이면 무작위로 정함
	float GetWeakLineAngle(const AActor* InActor);
	void RerollWeakLineAngle(const AActor* InActor);

	bool IsAimAlignedWithWeakLine(const APawn* InPawn);
#pragma endregion

	void CreateIndicator();
	void UpdateIndicator();
	void DestroyIndicator();

	void StartSlashTrail();
	void StopSlashTrail();

	// InRotation은 판정 상자 회전(X = 카메라 앞, Y = 베는 선, Z = 평면 법선). 약점을 베었으면 빛줄기 색을 바꿈.
	// InOrigin이 있으면 그 위치에 그림(베는 평면 밖에 있는 적의 약점 베기)
	void SpawnSlashMark(const FQuat& InRotation, bool bWeakPointHit, const FVector* InOrigin = nullptr);

	void QueueSlashSparks(const FQuat& InRotation);

	// bSpawnAll이면 시각과 관계없이 남은 불꽃을 모두 터뜨림(블레이드 모드가 끝날 때)
	void SpawnPendingSparks(bool bSpawnAll);

	FVector GetSlashLineOrigin(const FQuat& InRotation);

	// 수명이 다한 자국을 지움
	void RemoveExpiredSlashMarks();

	// 블레이드 모드가 끝날 때 남은 자국은 능력 틱 없이도 지워지도록 타이머에 맡김
	void HandOffSlashMarks();

	// 인디케이터·자국이 같은 평면 메시를 씀
	UStaticMesh* GetPlaneMesh() const;

	// 궤적 종료·인디케이터 번쩍임을 실제 시간으로 줄여 나감
	void TickSlashEffects(float RealDeltaTime);

	void TickCameraShake(float RealDeltaTime);
	void ResetCameraShake();

	void BeginBladeMode();
	void RestoreBladeMode();

	// 실제 시간으로 쌓아 StaminaDrainInterval마다 깎음. 스태미나가 바닥나면 true
	bool TickStaminaDrain(float RealDeltaTime);

	// 스태미나를 다 썼을 때: 디버프를 걸고 블레이드 모드를 끝냄
	void ExhaustBladeMode();

#pragma region Input
	// 블레이드 모드 동안만 켜는 매핑. 마우스 이동을 조준, 왼쪽 클릭을 베기로 씀(기본 매핑의 시점·공격보다 우선)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Input")
	TObjectPtr<UInputMappingContext> BladeModeMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Input")
	int32 BladeModeMappingPriority = 4;

	// Axis2D(마우스 이동량)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Input")
	TObjectPtr<UInputAction> SlashAction;

	// 지금 각도에 CrossSlashAngleOffset을 더한 각도로 바로 벰(기본 90도 = 지금 각도와 엇갈리는 베기)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Input")
	TObjectPtr<UInputAction> CrossSlashAction;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Input")
	float CrossSlashAngleOffset = 90.0f;
#pragma endregion

#pragma region Aim
	// 마우스 이동량을 가상 스틱에 더하는 배율. 가상 스틱의 방향이 베는 각도가 됨
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Aim", meta = (ClampMin = "0.0"))
	float AimSensitivity = 0.05f;

	// 가상 스틱 길이가 이보다 짧으면 각도를 바꾸지 않음(살짝 흔들린 마우스로 칼이 돌지 않게)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimDeadZone = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Aim")
	bool bInvertAimY = false;

	// 칼 각도가 목표 각도를 따라가는 속도(실제 시간 기준, 초당 도)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Aim", meta = (ClampMin = "0.0"))
	float AimRotationSpeed = 900.0f;

	// 블레이드 모드에 들어갈 때 칼 각도(0 = 오른쪽 수평)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Aim")
	float InitialAimAngle = 0.0f;

	// 캐릭터가 카메라 방향으로 돌아서는 속도
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Aim", meta = (ClampMin = "0.0"))
	float FaceCameraInterpSpeed = 15.0f;
#pragma endregion

#pragma region TimeAndCamera
	// 블레이드 모드 동안 세상의 시간 배율. 플레이어 자신은 원래 속도로 움직임
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Time", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float WorldTimeDilation = 0.2f;

#pragma region Stamina
	// 발동 비용은 막기처럼 Cost/CostGameplayEffectClass(GE_BladeModeCost)로 처리. 아래는 유지하는 동안 깎는 양
	// 스태미나를 깎는 GE. Player.SetByCaller.StaminaDrain 값을 CurrentStamina에 더함(음수로 넘김)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Stamina")
	TSubclassOf<UGameplayEffect> StaminaDrainEffect;

	// 초당 깎는 스태미나(실제 시간 기준)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Stamina", meta = (ClampMin = "0.0"))
	float StaminaDrainPerSecond = 5.0f;

	// 깎는 간격(실제 시간, 초)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Stamina", meta = (ClampMin = "0.02"))
	float StaminaDrainInterval = 0.2f;

	// 스태미나를 다 쓰면 블레이드 모드를 강제로 끝내고 거는 디버프. 이 효과가 남아 있는 동안 다시 들어갈 수 없음
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Stamina")
	TSubclassOf<UGameplayEffect> ExhaustedEffect;
#pragma endregion

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "0.0"))
	float ZoomArmLength = 130.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float ZoomFieldOfView = 55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "0.0"))
	float CameraInterpSpeed = 12.0f;

	// 벨 때 카메라 흔들림. 엔진 카메라 셰이크는 월드 시간으로 돌아 슬로모션에서 5배 느려지므로, 실제 시간으로 직접 흔듦
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "0.0"))
	float SlashShakeDuration = 0.18f;

	// 흔들리는 거리(cm, 카메라 좌우·상하)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "0.0"))
	float SlashShakeAmplitude = 6.0f;

	// 흔들리는 기울기(도, 카메라 롤)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "0.0"))
	float SlashShakeRollAmplitude = 1.5f;

	// 흔들림 빈도(초당)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Camera", meta = (ClampMin = "0.0"))
	float SlashShakeFrequency = 35.0f;
#pragma endregion

#pragma region Slash
	// 베는 판정 상자: 카메라 앞쪽으로 길이 SlashRange, 베는 선을 따라 폭 SlashHalfWidth*2, 두께 SlashHalfThickness*2
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashRange = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashHalfWidth = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashHalfThickness = 20.0f;

	// 연속 클릭 간격(실제 시간, 초)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashBaseDamage = 10.0f;

	// 가로 베기는 범위가 넓어 많이 맞히므로 약하게, 세로 베기는 강하게. 각도에 따라 두 배율 사이를 곱셈 기준으로 보간(대각선 = 1배)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.01"))
	float HorizontalSlashDamageMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.01"))
	float VerticalSlashDamageMultiplier = 2.0f;

	// 맞은 적에게 보내는 히트리액션 이벤트(Shared.Event.HitReact 하위)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (Categories = "Shared.Event.HitReact"))
	FGameplayTag HitReactEventTag;

	// 베는 평면에 띄우는 이펙트(선택). X축이 카메라 앞, Y축이 베는 선 방향으로 회전해서 생성
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	TObjectPtr<UNiagaraSystem> SlashEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	bool bDrawDebugSlash = false;

	// 벤 자세에서 반대편 자세로 넘어가는 시간(실제 시간, 초). 0이면 바로 바뀜
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashPoseBlendTime = 0.12f;

	// 벨 때 베는 선을 따라 남기는 빛줄기(카메라를 향한 가는 띠). 머티리얼이 StartRealTime부터 실제 시간으로 쓸고 지나간 뒤 사라짐
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	TObjectPtr<UMaterialInterface> SlashMarkMaterial;

	// 빛줄기를 지우기까지의 시간(실제 시간, 초). 머티리얼의 SweepTime + FadeTime보다 길게
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashMarkLifetime = 0.45f;

	// 빛줄기·불꽃을 그리는 선의 위치(캐릭터에서 카메라 앞쪽으로 얼마나), 빛줄기 두께
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashLineForwardOffset = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.1"))
	float SlashLineWidth = 6.0f;

	// 베는 선을 따라 차례로 터뜨리는 불꽃. 비우면 쓰지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	TObjectPtr<UNiagaraSystem> SlashSparkEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0"))
	int32 SlashSparkCount = 7;

	// 칼이 있던 쪽 끝에서 반대쪽 끝까지 불꽃이 퍼지는 시간(실제 시간, 초). 빛줄기 머티리얼의 SweepTime과 맞춤
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashSparkSweepTime = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashSparkScale = 1.0f;

	// 벨 때 칼에 붙이는 궤적(선택). 비워 두면 쓰지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	TObjectPtr<UNiagaraSystem> SlashTrailEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash")
	FName SlashTrailSocketName = TEXT("SwordTrailSocket");

	// 궤적을 켜 두는 시간(실제 시간, 초). 자세 블렌드보다 조금 길게 두면 끝까지 그려짐
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Slash", meta = (ClampMin = "0.0"))
	float SlashTrailDuration = 0.2f;
#pragma endregion

#pragma region Indicator
	// 블레이드 모드 동안 베는 평면을 보여주는 판. 판정 상자와 같은 위치·크기로 매 틱 칼 각도를 따라 돈다
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Indicator")
	bool bShowIndicator = true;

	// 100x100 평면(XY, 법선 +Z). 비우면 /Engine/BasicShapes/Plane
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Indicator")
	TObjectPtr<UStaticMesh> IndicatorMesh;

	// 양면 반투명 머티리얼 권장
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Indicator")
	TObjectPtr<UMaterialInterface> IndicatorMaterial;

	// 벨 때 인디케이터가 번쩍이는 밝기 배율과 시간(실제 시간, 초). 머티리얼의 Intensity 파라미터를 곱함
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Indicator", meta = (ClampMin = "1.0"))
	float IndicatorFlashMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Indicator", meta = (ClampMin = "0.0"))
	float IndicatorFlashTime = 0.15f;

#pragma region WeakLine
	// 블레이드 모드 대상(화면 중앙에 가장 가까운 적) 몸에 약점 선을 그림. 칼 각도를 선에 맞춰 베면 추가 데미지를 주고 선이 바뀜
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line")
	bool bUseWeakLine = true;

	// 대상 메시에 거는 오버레이 머티리얼. WeakPlanePoint·WeakPlaneNormal(벡터), Width·Intensity(스칼라)를 씀. 비우면 M_BladeWeakLine
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line")
	TObjectPtr<UMaterialInterface> WeakLineMaterial;

	// 칼 각도와 약점 선 각도 차이가 이 안이면 약점 베기(도)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float WeakLineTolerance = 15.0f;

	// 허용 범위 밖에서 이만큼 멀어질 때까지 선이 점점 흐려짐(도)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "1.0"))
	float WeakLineFadeRange = 45.0f;

	// 새 약점 선은 이전 선과 최소 이만큼 다른 각도(도)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float WeakLineMinRerollDelta = 45.0f;

	// 약점 베기 데미지 배율(각도 배율에 곱함)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "1.0"))
	float WeakPointDamageMultiplier = 3.0f;

	// 카메라 정면에서 이 각도 안에 있는 적만 대상으로 고름(도)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float WeakLineTargetConeAngle = 35.0f;

	// 선 두께(cm, 중심에서 한쪽)
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "0.1"))
	float WeakLineWidth = 3.0f;

	// 각도가 멀 때 / 허용 범위 안일 때 선 밝기
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "0.0"))
	float WeakLineFaintIntensity = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line", meta = (ClampMin = "0.0"))
	float WeakLineAlignedIntensity = 5.0f;

	// 허용 범위 안일 때 인디케이터 색, 약점을 베었을 때 빛줄기 색
	UPROPERTY(EditDefaultsOnly, Category = "Blade Mode|Weak Line")
	FLinearColor WeakLineAlignedColor = FLinearColor(1.0f, 0.65f, 0.1f, 1.0f);
#pragma endregion

	// 약점 선을 그리고 있는 적 전부
	UPROPERTY(Transient)
	TArray<FBladeWeakLineTarget> WeakLineTargets;

	// 베는 평면 축의 기준이 되는 적(WeakLineTargets 중 화면 중앙에 가장 가까운 적, 유효한 동안 유지)
	TWeakObjectPtr<APawn> WeakLineTarget;

	// 적마다 약점 각도. 블레이드 모드를 나갔다 들어와도 유지(어빌리티가 액터마다 하나라 계속 남음)
	TMap<TWeakObjectPtr<const AActor>, float> WeakLineAngles;

	FLinearColor IndicatorBaseColor = FLinearColor::White;

	// GetSlashLineOrigin이 빛줄기·불꽃을 베는 평면 위로 옮길 때 쓰는 평면 위의 점
	FVector LastSlashPlanePoint = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> IndicatorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SlashTrailComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SlashMarks;

	// SlashMarks와 같은 순서. 지울 실제 시각(FPlatformTime::Seconds 기준)
	TArray<double> SlashMarkExpireTimes;

	// 아직 터뜨리지 않은 불꽃(위치·회전·터뜨릴 실제 시각)
	TArray<FTransform> PendingSparkTransforms;
	TArray<double> PendingSparkTimes;
#pragma endregion

	// 가상 스틱(-1~1). 마우스 이동을 쌓아 방향을 만든다
	FVector2D AimStick = FVector2D::ZeroVector;
	float TargetAimAngle = 0.0f;
	float CurrentAimAngle = 0.0f;
	double LastSlashRealTime = -1.0;

	TArray<uint32> InputBindingHandles;

	TWeakObjectPtr<USpringArmComponent> CachedCameraBoom;
	TWeakObjectPtr<UCameraComponent> CachedFollowCamera;
	TWeakObjectPtr<UWarriorAnimInstance> CachedAnimInstance;

	float SavedArmLength = 0.0f;
	float SavedFieldOfView = 90.0f;
	float SavedPlayerTimeDilation = 1.0f;
	float SlashTrailRemaining = 0.0f;
	float StaminaDrainAccumulator = 0.0f;
	float ShakeRemaining = 0.0f;
	float ShakeTime = 0.0f;
	FVector SavedCameraRelativeLocation = FVector::ZeroVector;
	FRotator SavedCameraRelativeRotation = FRotator::ZeroRotator;
	float IndicatorFlashRemaining = 0.0f;
	float IndicatorBaseIntensity = 1.0f;
	bool bBladeModeApplied = false;
};
