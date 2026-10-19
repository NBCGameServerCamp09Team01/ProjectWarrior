#pragma once

#include "CoreMinimal.h"
#include "ProjectWarrior/Components/PawnExtensionComponentBase.h"
#include "PlayerInteractionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTWARRIOR_API UPlayerInteractionComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	void TryInteract();

	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 카메라에서 쏘는 트레이스 길이 (카메라~캐릭터 거리 포함)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float TraceDistance = 1000.f;

	// 캐릭터에서 대상까지 허용 거리
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float CheckInterval = 0.1f;

private:
	void UpdateFocus();
	void SetFocusedActor(AActor* InNewActor);

	TWeakObjectPtr<AActor> FocusedActor;

	FTimerHandle FocusTimerHandle;
};