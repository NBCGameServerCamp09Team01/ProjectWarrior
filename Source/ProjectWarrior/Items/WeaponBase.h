// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectWarrior/Types/WarriorStructTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "WeaponBase.generated.h"

class UBoxComponent;

DECLARE_DELEGATE_OneParam(FOnTargetInteractedDelegate, AActor*)

UCLASS()
class PROJECTWARRIOR_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeaponBase(const FObjectInitializer& ObjectInitializer);

	static inline const FName WeaponMeshComponentName = TEXT("WeaponMesh");

	FOnTargetInteractedDelegate OnWeaponHitTarget;
	FOnTargetInteractedDelegate OnWeaponPulledFromTarget;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponData")
	FWarriorPlayerWeaponData PlayerWeaponData;

	UFUNCTION(BlueprintCallable)
	void AssignGrantedAbilitySpecHandles(const TArray<FGameplayAbilitySpecHandle>& InSpecHandles);

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAbilitySpecHandle> GetGrantedAbilitySpecHandles() const;

private:
	TArray<FGameplayAbilitySpecHandle> GrantedAbilitySpecHandles;

	// 주인 캐릭터가 제거되면(시체가 SetLifeSpan으로 사라질 때 등) 무기도 함께 제거. 장착 여부와 무관
	UFUNCTION()
	void HandleOwningActorDestroyed(AActor* DestroyedActor);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UBoxComponent* WeaponCollision;

	UFUNCTION()
	virtual void OnCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnCollisionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


public:
	FORCEINLINE UBoxComponent* GetWeaponCollision() const { return WeaponCollision; }
	FORCEINLINE UMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

};
