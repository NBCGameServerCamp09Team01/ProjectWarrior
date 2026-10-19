#include "PlayerInteractionComponent.h"
#include "ProjectWarrior/Interfaces/WarriorInteractableInterface.h"

void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->GetTimerManager().SetTimer(FocusTimerHandle, this, &ThisClass::UpdateFocus, CheckInterval, true);
}

void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(FocusTimerHandle);
	SetFocusedActor(nullptr);

	Super::EndPlay(EndPlayReason);
}

void UPlayerInteractionComponent::TryInteract()
{
	AActor* Target = FocusedActor.Get();
	IWarriorInteractableInterface* Interactable = Cast<IWarriorInteractableInterface>(Target);
	if (!Interactable || !Interactable->CanInteract(GetOwningPawn()))
	{
		return;
	}

	Interactable->Interact(GetOwningPawn());
	SetFocusedActor(nullptr);
}

void UPlayerInteractionComponent::UpdateFocus()
{
	APawn* OwningPawn = GetOwningPawn();
	APlayerController* PC = GetOwningController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		SetFocusedActor(nullptr);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;

	// 자기 자신과 들고 있는 무기에 트레이스가 막히지 않게 제외
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), false, OwningPawn);
	TArray<AActor*> AttachedActors;
	OwningPawn->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	AActor* NewFocus = nullptr;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		const IWarriorInteractableInterface* Interactable = Cast<IWarriorInteractableInterface>(HitActor);
		const bool bInRange = FVector::Dist(OwningPawn->GetActorLocation(), Hit.ImpactPoint) <= InteractRange;

		if (Interactable && bInRange && Interactable->CanInteract(OwningPawn))
		{
			NewFocus = HitActor;
		}
	}

	SetFocusedActor(NewFocus);

	// 테스트용.
	//DrawDebugLine(GetWorld(), ViewLocation, TraceEnd, FColor::Green, false, CheckInterval);
}

void UPlayerInteractionComponent::SetFocusedActor(AActor* InNewActor)
{
	AActor* OldActor = FocusedActor.Get();
	if (OldActor == InNewActor)
	{
		return;
	}

	if (IWarriorInteractableInterface* Old = Cast<IWarriorInteractableInterface>(OldActor))
	{
		Old->SetInteractionFocus(false);
	}

	FocusedActor = InNewActor;

	if (IWarriorInteractableInterface* New = Cast<IWarriorInteractableInterface>(InNewActor))
	{
		New->SetInteractionFocus(true);
	}
}