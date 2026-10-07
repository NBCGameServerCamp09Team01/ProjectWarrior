// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "WarriorProjectileBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class PROJECTWARRIOR_API AWarriorProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AWarriorProjectileBase();

	// 발사한 어빌리티가 넘겨주는 데미지 Spec (어빌리티가 끝난 뒤에 맞아도 적용 가능)
	UPROPERTY(BlueprintReadWrite, Category = "Projectile", meta = (ExposeOnSpawn = "true"))
	FGameplayEffectSpecHandle ProjectileDamageEffectSpecHandle;

	// 스폰 직후에는 충돌/이동이 꺼진 대기 상태. 호출 시 부착을 풀고 LaunchDirection으로 발사
	// InDamageSpecHandle이 유효하면 기존 Spec을 덮어씀
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void LaunchProjectile(const FVector& LaunchDirection, const FGameplayEffectSpecHandle& InDamageSpecHandle);

	UFUNCTION(BlueprintPure, Category = "Projectile")
	bool IsLaunched() const { return bLaunched; }

	// 원래 발사자에게 되돌림. 이후 Reflector가 쏜 투사체로 취급하고, 피해는 같은 이펙트로 Reflector가 다시 만듦
	// 막기 이벤트(Player.Event.Successful.Block, OptionalObject = 이 투사체)를 받은 어빌리티에서 호출하면 막힌 투사체가 파괴되지 않음
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	bool ReflectProjectile(APawn* Reflector, float DamageMultiplier = 1.f, float SpeedMultiplier = 1.5f);

	UFUNCTION(BlueprintPure, Category = "Projectile")
	bool IsReflected() const { return bReflected; }

protected:
	virtual void Tick(float DeltaSeconds) override;

	// 발사 후 수명. 대기 중에는 사라지지 않음
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float LifeSpanAfterLaunch = 5.f;

	// 루트. +X가 발사 방향이며 ProjectileMovement가 이 컴포넌트를 움직임
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USceneComponent* ProjectileRoot;

	// 충돌 없음. 에셋 방향이 +X가 아니면 BP에서 상대 회전으로 맞춤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UStaticMeshComponent* ProjectileMesh;

	// 판정용 박스 (Overlap 전용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UBoxComponent* ProjectileCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovement;

	// 명중 시 대상에게 히트리액션 이벤트를 보낼지 (근접 공격 어빌리티와 동일한 흐름)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	bool bSendHitReactEvent = true;

	// 보낼 히트리액션 이벤트. 플레이어는 Light/Heavy를 구분해서 반응하고, 적은 강도와 무관하게 반응함
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (Categories = "Shared.Event.HitReact", EditCondition = "bSendHitReactEvent"))
	FGameplayTag HitReactEventTag;

	// 막기 규칙 (회피는 항상 가능). 저격 화살은 PerfectParryOnly
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	EWarriorBlockRule BlockRule = EWarriorBlockRule::Blockable;

	// ReflectProjectile로 되돌릴 수 있는지 (보스 투사체 등은 끌 수 있음)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Reflect")
	bool bCanBeReflected = true;

	UFUNCTION()
	virtual void OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 이펙트/사운드용. 파괴 직전에 호출됨
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Impact"))
	void BP_OnProjectileImpact(AActor* HitActor, const FVector& ImpactLocation, EWarriorHitResultType HitResult);

	// 되돌아갈 때 호출 (이펙트/사운드용). 투사체는 파괴되지 않고 계속 날아감
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Reflected"))
	void BP_OnProjectileReflected(AActor* Reflector, const FVector& ReflectLocation);

private:
	// 쏜 캐릭터, 그 캐릭터의 무기, 다른 투사체는 무시
	bool ShouldIgnoreActor(AActor* OtherActor) const;

	void HandleHitPawn(APawn* HitPawn, const FVector& ImpactLocation);
	void ApplyDamageToTarget(AActor* TargetActor);

	// 트레이스에 걸린 액터가 폰에 붙은 무기 등이면 그 폰을 반환
	static APawn* ResolveHitPawn(AActor* HitActor);

	bool bReflected = false;

	UPROPERTY()
	TArray<AActor*> ProcessedActors;

	bool bLaunched = false;

	FVector LastTraceLocation = FVector::ZeroVector;

public:
	FORCEINLINE UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
};
