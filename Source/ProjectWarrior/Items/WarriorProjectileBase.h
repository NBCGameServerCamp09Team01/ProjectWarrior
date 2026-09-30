// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ProjectWarrior/Types/WarriorEnumTypes.h"
#include "WarriorProjectileBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class PROJECTWARRIOR_API AWarriorProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AWarriorProjectileBase();

	// 발사한 어빌리티가 미리 만들어 넘겨주는 데미지 Spec (어빌리티가 끝난 뒤에 맞아도 적용 가능)
	UPROPERTY(BlueprintReadWrite, Category = "Projectile", meta = (ExposeOnSpawn = "true"))
	FGameplayEffectSpecHandle ProjectileDamageEffectSpecHandle;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* ProjectileCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovement;

	// 명중 시 대상에게 Shared.Event.HitReact를 보낼지 (근접 공격 어빌리티와 동일한 흐름)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	bool bSendHitReactEvent = true;

	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	virtual void OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 이펙트/사운드용. 파괴 직전에 호출됨
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Impact"))
	void BP_OnProjectileImpact(AActor* HitActor, const FVector& ImpactLocation, EWarriorHitResultType HitResult);

private:
	void HandleHitPawn(APawn* HitPawn, const FVector& ImpactLocation);
	void ApplyDamageToTarget(AActor* TargetActor);

	UPROPERTY()
	TArray<AActor*> ProcessedActors;

public:
	FORCEINLINE UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
};
