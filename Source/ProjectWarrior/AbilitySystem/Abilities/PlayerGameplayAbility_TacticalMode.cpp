// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerGameplayAbility_TacticalMode.h"
#include "PlayerGameplayAbility_TacticalTechnique.h"
#include "ProjectWarrior/Characters/WarriorPlayerCharacter.h"
#include "ProjectWarrior/Characters/WarriorAICharacter.h"
#include "ProjectWarrior/Controllers/WarriorPlayerController.h"
#include "ProjectWarrior/AbilitySystem/AbilityTask_ExecuteTaskOnTick.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/Widgets/TacticalMenuWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "Camera/CameraComponent.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "EngineUtils.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

#define LOCTEXT_NAMESPACE "TacticalMode"

UPlayerGameplayAbility_TacticalMode::UPlayerGameplayAbility_TacticalMode()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer TacticalAssetTags;
	TacticalAssetTags.AddTag(WarriorGameplayTags::Player_Ability_TacticalMode);
	SetAssetTags(TacticalAssetTags);

	ActivationOwnedTags.AddTag(WarriorGameplayTags::Player_Status_TacticalMode);
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Shared_Status_Death);

	// 블레이드 모드와 시간 배율을 서로 덮어쓰지 않도록 동시에 켜지 않음
	ActivationBlockedTags.AddTag(WarriorGameplayTags::Player_Status_BladeMode);
}

void UPlayerGameplayAbility_TacticalMode::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!GetPlayerCharacterFromActorInfo() || !GetPlayerControllerFromActorInfo() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 락온은 캐릭터 회전을 대상 쪽으로 고정해 전술 기술의 방향을 덮어씀 → 전술 모드를 켜면 해제(전술 모드 중에는 다시 켜지지 않음)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FGameplayTagContainer TargetLockTags(WarriorGameplayTags::Player_Ability_TargetLock);
		ASC->CancelAbilities(&TargetLockTags);
	}

	GrantTechniques();

	Stocks.Reset();
	Targets.Reset();
	Step = ETacticalModeStep::SelectCommand;
	CommandIndex = 0;
	TargetIndex = 0;
	ExecutingIndex = INDEX_NONE;
	PendingConfirmTime = -1.0;
	bWaitingForFinisher = false;

	BeginSelection();

	UAbilityTask_ExecuteTaskOnTick* TickTask = UAbilityTask_ExecuteTaskOnTick::ExecuteTaskOnTick(this);
	TickTask->OnAbilityTaskTick.AddDynamic(this, &ThisClass::OnTacticalTick);
	TickTask->ReadyForActivation();
}

void UPlayerGameplayAbility_TacticalMode::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndSelection();
	RestoreFrozenActors();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); ASC && AbilityEndedHandle.IsValid())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}

	AbilityEndedHandle.Reset();
	ExecutingTechniqueHandle = FGameplayAbilitySpecHandle();
	Stocks.Reset();
	Targets.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPlayerGameplayAbility_TacticalMode::BeginSelection()
{
	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (TacticalMappingContext)
		{
			Subsystem->AddMappingContext(TacticalMappingContext, TacticalMappingPriority);
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerCharacter->InputComponent))
	{
		if (UpAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(UpAction, ETriggerEvent::Started, this, &ThisClass::OnUpInput).GetHandle());
		}

		if (DownAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(DownAction, ETriggerEvent::Started, this, &ThisClass::OnDownInput).GetHandle());
		}

		if (LeftAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(LeftAction, ETriggerEvent::Started, this, &ThisClass::OnLeftInput).GetHandle());
		}

		if (RightAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(RightAction, ETriggerEvent::Started, this, &ThisClass::OnRightInput).GetHandle());
		}

		if (ConfirmAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ThisClass::OnConfirmInput).GetHandle());
		}

		if (BackAction)
		{
			InputBindingHandles.Add(EnhancedInput->BindAction(BackAction, ETriggerEvent::Started, this, &ThisClass::OnBackInput).GetHandle());
		}
	}

	// 고르는 동안은 걷지도, 시점을 돌리지도 않음(대상을 고르면 카메라가 알아서 돎)
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);

	// 플레이어를 포함해 세상 전체를 느리게(블레이드 모드와 달리 플레이어도 같이 느려짐)
	// 히트스톱 도중에 켜면 지금 값이 비정상일 수 있으므로 플레이어 배율은 1을 기준으로 씀(블레이드 모드와는 동시에 켜지지 않음)
	SavedPlayerTimeDilation = 1.0f;
	PlayerCharacter->CustomTimeDilation = SavedPlayerTimeDilation;
	UGameplayStatics::SetGlobalTimeDilation(PlayerCharacter, TimeDilation);

	CachedCamera = PlayerCharacter->FindComponentByClass<UCameraComponent>();

	if (UCameraComponent* Camera = CachedCamera.Get())
	{
		bSavedOverrideMotionBlur = Camera->PostProcessSettings.bOverride_MotionBlurAmount;
		SavedMotionBlurAmount = Camera->PostProcessSettings.MotionBlurAmount;
		Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Camera->PostProcessSettings.MotionBlurAmount = 0.0f;
	}

	if (MenuWidgetClass)
	{
		MenuWidget = CreateWidget<UTacticalMenuWidget>(PlayerController, MenuWidgetClass);

		if (MenuWidget)
		{
			MenuWidget->AddToViewport(5);
		}
	}

	// 처음 커서는 넣을 수 있는 첫 기술에
	for (int32 Index = 0; Index < Techniques.Num(); ++Index)
	{
		if (GetTechniqueCost(Techniques[Index]) <= GetRemainingStamina())
		{
			CommandIndex = Index;
			break;
		}
	}

	bSelectionApplied = true;
	RefreshMenu();
}

void UPlayerGameplayAbility_TacticalMode::EndSelection()
{
	if (!bSelectionApplied)
	{
		return;
	}

	bSelectionApplied = false;
	PendingConfirmTime = -1.0;

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

	if (UCameraComponent* Camera = CachedCamera.Get())
	{
		Camera->PostProcessSettings.bOverride_MotionBlurAmount = bSavedOverrideMotionBlur;
		Camera->PostProcessSettings.MotionBlurAmount = SavedMotionBlurAmount;
	}

	CachedCamera = nullptr;
	InputBindingHandles.Reset();

	if (PlayerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (TacticalMappingContext)
			{
				Subsystem->RemoveMappingContext(TacticalMappingContext);
			}
		}

		PlayerController->ResetIgnoreMoveInput();
		PlayerController->ResetIgnoreLookInput();
	}

	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}
}

void UPlayerGameplayAbility_TacticalMode::GrantTechniques()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	if (!ASC)
	{
		return;
	}

	// 기술은 전술 모드에서만 쓰므로 무기 어빌리티 목록 대신 여기서 처음 켤 때 부여
	for (const TSubclassOf<UPlayerGameplayAbility_TacticalTechnique>& Technique : Techniques)
	{
		if (Technique && !ASC->FindAbilitySpecFromClass(Technique))
		{
			FGameplayAbilitySpec TechniqueSpec(Technique, GetAbilityLevel());
			TechniqueSpec.SourceObject = GetAvatarActorFromActorInfo();
			ASC->GiveAbility(TechniqueSpec);
		}
	}
}

float UPlayerGameplayAbility_TacticalMode::GetTechniqueCost(const TSubclassOf<UPlayerGameplayAbility_TacticalTechnique>& InTechnique) const
{
	// Cost는 비용 GE가 CurrentStamina에 더하는 값이라 음수로 넣음(GE_BlockCost 등과 같은 규칙) → 크기만 씀
	return InTechnique ? FMath::Abs(InTechnique->GetDefaultObject<UPlayerGameplayAbility_TacticalTechnique>()->GetCost()) : 0.0f;
}

float UPlayerGameplayAbility_TacticalMode::GetRemainingStamina()
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	float Remaining = ASC ? ASC->GetNumericAttribute(UWarriorAttributeSet::GetCurrentStaminaAttribute()) : 0.0f;

	// 스태미나는 실행할 때 내므로, 이미 넣은 스톡 비용을 빼서 남은 양을 셈
	for (const FTacticalStock& Stock : Stocks)
	{
		Remaining -= GetTechniqueCost(Stock.Technique);
	}

	return Remaining;
}

bool UPlayerGameplayAbility_TacticalMode::CanAffordAnyTechnique()
{
	const float Remaining = GetRemainingStamina();

	for (const TSubclassOf<UPlayerGameplayAbility_TacticalTechnique>& Technique : Techniques)
	{
		if (Technique && GetTechniqueCost(Technique) <= Remaining)
		{
			return true;
		}
	}

	return false;
}

bool UPlayerGameplayAbility_TacticalMode::IsValidTarget(AActor* InActor)
{
	APawn* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	APawn* TargetPawn = Cast<APawn>(InActor);

	// 적대·살아 있음·처형당하는 중 아님·스폰 직후 초기화 중 아님
	return PlayerCharacter && TargetPawn
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPawn)
		&& UWarriorFunctionLibrary::IsTargetPawnHostile(PlayerCharacter, TargetPawn)
		&& !UWarriorFunctionLibrary::IsActorDead(TargetPawn)
		&& !UWarriorFunctionLibrary::NativeDoesActorHaveTag(TargetPawn, WarriorGameplayTags::Shared_Status_Finisher)
		&& !UWarriorFunctionLibrary::NativeDoesActorHaveTag(TargetPawn, WarriorGameplayTags::Shared_Status_Initializing);
}

void UPlayerGameplayAbility_TacticalMode::PruneInvalidTargets()
{
	bool bChanged = false;

	// 이미 넣은 스톡의 대상이 죽거나 처형당하면 그 칸을 뺌(예약한 스태미나도 돌아옴)
	const int32 RemovedStocks = Stocks.RemoveAll([this](const FTacticalStock& Stock) { return !IsValidTarget(Stock.Target.Get()); });
	bChanged |= RemovedStocks > 0;

	// 대상 목록 중 하나라도 무효가 되면 다시 모음. 고르던 대상이 아직 유효하면 그대로 고른 채로
	if (Step == ETacticalModeStep::SelectTarget
		&& Targets.ContainsByPredicate([this](const TWeakObjectPtr<AActor>& Target) { return !IsValidTarget(Target.Get()); }))
	{
		AActor* SelectedTarget = Targets.IsValidIndex(TargetIndex) ? Targets[TargetIndex].Get() : nullptr;
		GatherTargets();

		const int32 KeptIndex = Targets.IndexOfByKey(SelectedTarget);
		if (SelectedTarget && KeptIndex != INDEX_NONE)
		{
			TargetIndex = KeptIndex;
		}

		if (Targets.IsEmpty())
		{
			Step = ETacticalModeStep::SelectCommand;
		}

		bChanged = true;
	}

	if (bChanged)
	{
		RefreshMenu();
	}
}

void UPlayerGameplayAbility_TacticalMode::GatherTargets()
{
	Targets.Reset();
	TargetNames.Reset();

	AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo();
	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();
	UWorld* World = GetWorld();

	if (!PlayerCharacter || !PlayerController || !World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TacticalModeTargets), false, PlayerCharacter);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, PlayerCharacter->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(TargetRange), QueryParams);

	TArray<TPair<float, AActor*>> SortedTargets;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Candidate = Cast<APawn>(Overlap.GetActor());

		if (!Candidate || SortedTargets.ContainsByPredicate([Candidate](const TPair<float, AActor*>& Entry) { return Entry.Value == Candidate; })
			|| !IsValidTarget(Candidate))
		{
			continue;
		}

		// 화면 왼쪽부터. 화면 밖(뒤쪽)은 맨 끝
		FVector2D ScreenPosition;
		const bool bOnScreen = PlayerController->ProjectWorldLocationToScreen(Candidate->GetActorLocation(), ScreenPosition);
		SortedTargets.Emplace(bOnScreen ? ScreenPosition.X : TNumericLimits<float>::Max(), Candidate);
	}

	SortedTargets.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key < B.Key; });

	// 처음 커서는 화면 중앙에 가장 가까운 대상
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	float BestDistance = TNumericLimits<float>::Max();
	TargetIndex = 0;

	// 같은 이름이 몇 번째인지 세어 "이름", "이름 (1)", "이름 (2)"
	TMap<FString, int32> NameCounts;

	for (int32 Index = 0; Index < SortedTargets.Num(); ++Index)
	{
		AActor* Target = SortedTargets[Index].Value;
		Targets.Add(Target);

		const AWarriorAICharacter* TargetAI = Cast<AWarriorAICharacter>(Target);
		const FText BaseName = TargetAI ? TargetAI->GetDisplayName() : FText::FromString(Target->GetName());
		int32& Count = NameCounts.FindOrAdd(BaseName.ToString());
		TargetNames.Add(Count == 0 ? BaseName : FText::Format(LOCTEXT("NumberedTarget", "{0} ({1})"), BaseName, FText::AsNumber(Count)));
		++Count;

		const float Distance = FMath::Abs(SortedTargets[Index].Key - ViewportX * 0.5f);

		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			TargetIndex = Index;
		}
	}
}

void UPlayerGameplayAbility_TacticalMode::OnTacticalTick(float DeltaTime)
{
	// 더블탭이 아니었던 단일 좌클릭을 이제 처리
	if (PendingConfirmTime >= 0.0 && FPlatformTime::Seconds() - PendingConfirmTime > DoubleTapWindow)
	{
		PendingConfirmTime = -1.0;
		HandleConfirm();
	}

	if (bSelectionApplied)
	{
		PruneInvalidTargets();
	}

	// 고르는 동안은 시간 배율을 유지. 피니시 등 진행 중이던 동작의 히트스톱(GA_HitStop)이 끝나면서 시간을 1배로 되돌리기 때문
	if (bSelectionApplied)
	{
		if (AWarriorPlayerCharacter* PlayerCharacter = GetPlayerCharacterFromActorInfo())
		{
			if (!FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(PlayerCharacter), TimeDilation))
			{
				UGameplayStatics::SetGlobalTimeDilation(PlayerCharacter, TimeDilation);
			}

			if (!FMath::IsNearlyEqual(PlayerCharacter->CustomTimeDilation, SavedPlayerTimeDilation))
			{
				PlayerCharacter->CustomTimeDilation = SavedPlayerTimeDilation;
			}
		}
	}

	// 세상이 느려도 카메라는 실제 시간으로 돎
	TickTargetFocus(FApp::GetDeltaTime());

	if (Step == ETacticalModeStep::Executing)
	{
		if (bWaitingForFinisher)
		{
			if (!IsWaitingForFinisher())
			{
				BeginExecution();
			}
		}
		else
		{
			TickExecutionFreeze();
		}
	}
}

void UPlayerGameplayAbility_TacticalMode::TickExecutionFreeze()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// 새로 생긴 적·투사체도 잡히도록 매 틱 훑음(실행은 몇 초뿐)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		AWarriorAICharacter* EnemyAI = Cast<AWarriorAICharacter>(Actor);

		if (!EnemyAI && !Cast<AWarriorAICharacter>(Actor->GetInstigator()))
		{
			continue;
		}

		const float* SavedDilation = FrozenActorSavedDilations.Find(Actor);

		if (!SavedDilation)
		{
			SavedDilation = &FrozenActorSavedDilations.Add(Actor, Actor->CustomTimeDilation);
		}

		// 맞고 있는 적과 죽은 적(쓰러지는 모션)은 원래 속도
		const double* FreeUntil = UnfrozenUntil.Find(Actor);
		const bool bFree = (FreeUntil && *FreeUntil > Now) || (EnemyAI && UWarriorFunctionLibrary::IsActorDead(EnemyAI));
		const float DesiredDilation = bFree ? *SavedDilation : ExecutionEnemyTimeDilation;

		if (!FMath::IsNearlyEqual(Actor->CustomTimeDilation, DesiredDilation))
		{
			Actor->CustomTimeDilation = DesiredDilation;
		}
	}
}

void UPlayerGameplayAbility_TacticalMode::RestoreFrozenActors()
{
	for (const TPair<TWeakObjectPtr<AActor>, float>& Entry : FrozenActorSavedDilations)
	{
		if (AActor* Actor = Entry.Key.Get())
		{
			Actor->CustomTimeDilation = Entry.Value;
		}
	}

	FrozenActorSavedDilations.Reset();
	UnfrozenUntil.Reset();
}

void UPlayerGameplayAbility_TacticalMode::OnExecutionMeleeHit(FGameplayEventData Payload)
{
	AActor* HitActor = const_cast<AActor*>(Payload.Target.Get());

	if (!HitActor)
	{
		return;
	}

	// 피격 모션이 바로 나오도록 이 틱에 풀어 줌
	UnfrozenUntil.Add(HitActor, FPlatformTime::Seconds() + HitUnfreezeDuration);

	if (const float* SavedDilation = FrozenActorSavedDilations.Find(HitActor))
	{
		HitActor->CustomTimeDilation = *SavedDilation;
	}
}

void UPlayerGameplayAbility_TacticalMode::TickTargetFocus(float RealDeltaTime)
{
	if (Step != ETacticalModeStep::SelectTarget)
	{
		return;
	}

	AWarriorPlayerController* PlayerController = GetPlayerControllerFromActorInfo();
	const AActor* Target = Targets.IsValidIndex(TargetIndex) ? Targets[TargetIndex].Get() : nullptr;

	if (!PlayerController || !Target)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 대상 쪽을 보되 높이는 고정 각도로(가까운 대상에서 카메라가 바닥을 보지 않게)
	const FRotator CurrentRotation = PlayerController->GetControlRotation();
	FRotator DesiredRotation = (Target->GetActorLocation() - ViewLocation).Rotation();
	DesiredRotation.Pitch = TargetFocusPitch;
	DesiredRotation.Roll = CurrentRotation.Roll;

	PlayerController->SetControlRotation(FMath::RInterpTo(CurrentRotation, DesiredRotation, RealDeltaTime, TargetFocusInterpSpeed));
}

void UPlayerGameplayAbility_TacticalMode::OnUpInput(const FInputActionValue& InputActionValue)
{
	MoveCommandSelection(-1);
}

void UPlayerGameplayAbility_TacticalMode::OnDownInput(const FInputActionValue& InputActionValue)
{
	MoveCommandSelection(1);
}

void UPlayerGameplayAbility_TacticalMode::OnLeftInput(const FInputActionValue& InputActionValue)
{
	MoveTargetSelection(-1);
}

void UPlayerGameplayAbility_TacticalMode::OnRightInput(const FInputActionValue& InputActionValue)
{
	MoveTargetSelection(1);
}

void UPlayerGameplayAbility_TacticalMode::MoveCommandSelection(int32 InDelta)
{
	if (Step != ETacticalModeStep::SelectCommand || Techniques.IsEmpty())
	{
		return;
	}

	CommandIndex = (CommandIndex + InDelta + Techniques.Num()) % Techniques.Num();
	RefreshMenu();
}

void UPlayerGameplayAbility_TacticalMode::MoveTargetSelection(int32 InDelta)
{
	if (Step != ETacticalModeStep::SelectTarget || Targets.IsEmpty())
	{
		return;
	}

	TargetIndex = (TargetIndex + InDelta + Targets.Num()) % Targets.Num();
	RefreshMenu();
}

void UPlayerGameplayAbility_TacticalMode::OnConfirmInput(const FInputActionValue& InputActionValue)
{
	if (Step == ETacticalModeStep::Executing)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// 더블탭: 기다리던 단일 클릭을 버리고 바로 실행
	if (PendingConfirmTime >= 0.0 && Now - PendingConfirmTime <= DoubleTapWindow)
	{
		PendingConfirmTime = -1.0;

		if (!Stocks.IsEmpty())
		{
			ExecuteStocks();
		}

		return;
	}

	PendingConfirmTime = Now;
}

void UPlayerGameplayAbility_TacticalMode::HandleConfirm()
{
	if (Step == ETacticalModeStep::SelectCommand)
	{
		if (!Techniques.IsValidIndex(CommandIndex) || GetTechniqueCost(Techniques[CommandIndex]) > GetRemainingStamina())
		{
			return;
		}

		GatherTargets();

		if (Targets.IsEmpty())
		{
			if (MenuWidget)
			{
				MenuWidget->SetHint(LOCTEXT("NoTarget", "범위 안에 대상이 없음"));
			}

			return;
		}

		Step = ETacticalModeStep::SelectTarget;
		RefreshMenu();
		return;
	}

	if (Step == ETacticalModeStep::SelectTarget)
	{
		AActor* Target = Targets.IsValidIndex(TargetIndex) ? Targets[TargetIndex].Get() : nullptr;

		if (!Target || !IsValidTarget(Target))
		{
			return;
		}

		FTacticalStock& NewStock = Stocks.AddDefaulted_GetRef();
		NewStock.Technique = Techniques[CommandIndex];
		NewStock.Target = Target;

		Step = ETacticalModeStep::SelectCommand;

		if (AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			FGameplayEventData StockEventData;
			StockEventData.Instigator = Avatar;
			StockEventData.Target = Target;
			StockEventData.EventMagnitude = Stocks.Num();
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, WarriorGameplayTags::Player_Event_TacticalMode_StockAdded, StockEventData);
		}

		// 다 찼거나 남은 스태미나로 넣을 기술이 없으면 바로 실행
		if (Stocks.Num() >= MaxStocks || !CanAffordAnyTechnique())
		{
			ExecuteStocks();
			return;
		}

		RefreshMenu();
	}
}

void UPlayerGameplayAbility_TacticalMode::OnBackInput(const FInputActionValue& InputActionValue)
{
	PendingConfirmTime = -1.0;

	if (Step == ETacticalModeStep::SelectTarget)
	{
		Step = ETacticalModeStep::SelectCommand;
		RefreshMenu();
	}
	else if (Step == ETacticalModeStep::SelectCommand)
	{
		if (!Stocks.IsEmpty())
		{
			Stocks.Pop();
			RefreshMenu();
		}
		else
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
	}
}

void UPlayerGameplayAbility_TacticalMode::RefreshMenu()
{
	if (!MenuWidget)
	{
		return;
	}

	const float Remaining = GetRemainingStamina();
	TArray<FTacticalMenuEntry> Entries;

	if (Step == ETacticalModeStep::SelectTarget)
	{
		// 같은 패널에 대상 목록(오른쪽은 거리)
		const AActor* Avatar = GetAvatarActorFromActorInfo();

		for (int32 Index = 0; Index < Targets.Num(); ++Index)
		{
			FTacticalMenuEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Name = TargetNames.IsValidIndex(Index) ? TargetNames[Index] : FText::GetEmpty();

			if (const AActor* Target = Targets[Index].Get(); Target && Avatar)
			{
				Entry.Detail = FText::Format(LOCTEXT("TargetDistance", "{0}m"), FText::AsNumber(FMath::RoundToInt(FVector::Dist(Avatar->GetActorLocation(), Target->GetActorLocation()) / 100.0f)));
			}
		}

		MenuWidget->SetEntries(LOCTEXT("TargetsHeader", "Targets"), Entries, TargetIndex);
	}
	else
	{
		for (const TSubclassOf<UPlayerGameplayAbility_TacticalTechnique>& Technique : Techniques)
		{
			FTacticalMenuEntry& Entry = Entries.AddDefaulted_GetRef();

			if (Technique)
			{
				const float TechniqueCost = GetTechniqueCost(Technique);
				Entry.Name = Technique->GetDefaultObject<UPlayerGameplayAbility_TacticalTechnique>()->GetDisplayName();
				Entry.Detail = FText::AsNumber(FMath::RoundToInt(TechniqueCost));
				Entry.bEnabled = TechniqueCost <= Remaining;
			}
		}

		MenuWidget->SetEntries(LOCTEXT("CommandsHeader", "Commands"), Entries, CommandIndex);
	}

	TArray<FText> StockLines;

	for (const FTacticalStock& Stock : Stocks)
	{
		const FText TechniqueName = Stock.Technique ? Stock.Technique->GetDefaultObject<UPlayerGameplayAbility_TacticalTechnique>()->GetDisplayName() : FText::GetEmpty();
		const AWarriorAICharacter* StockTarget = Cast<AWarriorAICharacter>(Stock.Target.Get());
		StockLines.Add(StockTarget ? FText::Format(LOCTEXT("StockEntry", "{0} → {1}"), TechniqueName, StockTarget->GetDisplayName()) : TechniqueName);
	}

	MenuWidget->SetStocks(StockLines, MaxStocks);

	const FText StaminaText = FText::Format(LOCTEXT("Stamina", "스태미나 {0}"), FText::AsNumber(FMath::FloorToInt(Remaining)));

	if (Step == ETacticalModeStep::SelectTarget)
	{
		MenuWidget->SetHint(FText::Format(LOCTEXT("TargetHint", "A/D 대상 이동 · 좌클릭 확정 · 우클릭 뒤로   {0}"), StaminaText));
		MenuWidget->SetTarget(Targets.IsValidIndex(TargetIndex) ? Targets[TargetIndex].Get() : nullptr);
	}
	else
	{
		MenuWidget->SetHint(FText::Format(LOCTEXT("CommandHint", "W/S 이동 · 좌클릭 선택 · 더블클릭 실행 · 우클릭 취소   {0}"), StaminaText));
		MenuWidget->SetTarget(nullptr);
	}
}

void UPlayerGameplayAbility_TacticalMode::ExecuteStocks()
{
	Step = ETacticalModeStep::Executing;
	EndSelection();

	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, WarriorGameplayTags::Shared_Event_MeleeHit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnExecutionMeleeHit);
	HitTask->ReadyForActivation();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::OnAbilityEnded);
	}

	// 피니시 도중이면 피니시를 끝까지 보여 준 뒤 시작(기술 몽타주가 피니시를 끊지 않게). 적도 그때 멈춤(피니시는 적과 맞물린 동작)
	if (IsWaitingForFinisher())
	{
		bWaitingForFinisher = true;
		return;
	}

	BeginExecution();
}

bool UPlayerGameplayAbility_TacticalMode::IsWaitingForFinisher()
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC && ASC->HasMatchingGameplayTag(WarriorGameplayTags::Shared_Status_Finisher);
}

void UPlayerGameplayAbility_TacticalMode::BeginExecution()
{
	bWaitingForFinisher = false;

	// 세상 시간은 1배로 돌아오고, 적만 멈춘 채 플레이어가 차례로 행동(턴제)
	FrozenActorSavedDilations.Reset();
	UnfrozenUntil.Reset();
	TickExecutionFreeze();

	ExecutingIndex = -1;
	RunNextStock();
}

void UPlayerGameplayAbility_TacticalMode::RunNextStock()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	while (ASC && Stocks.IsValidIndex(++ExecutingIndex))
	{
		const FTacticalStock& Stock = Stocks[ExecutingIndex];
		AActor* Target = Stock.Target.Get();
		const FGameplayAbilitySpec* TechniqueSpec = Stock.Technique ? ASC->FindAbilitySpecFromClass(Stock.Technique) : nullptr;

		// 그 사이 죽었거나 처형당하는 중인 대상은 건너뜀
		if (!Target || !TechniqueSpec || !IsValidTarget(Target))
		{
			UE_LOG(LogProjectWarrior, Log, TEXT("[TacticalMode] stock %d skipped (target %s, spec %s)."),
				ExecutingIndex, *GetNameSafe(Target), TechniqueSpec ? TEXT("ok") : TEXT("missing"));
			continue;
		}

		UE_LOG(LogProjectWarrior, Log, TEXT("[TacticalMode] stock %d/%d: %s -> %s"), ExecutingIndex + 1, Stocks.Num(), *GetNameSafe(Stock.Technique), *GetNameSafe(Target));

		FGameplayEventData Payload;
		Payload.Instigator = GetAvatarActorFromActorInfo();
		Payload.Target = Target;

		ExecutingTechniqueHandle = TechniqueSpec->Handle;

		if (ASC->TriggerAbilityFromGameplayEvent(TechniqueSpec->Handle, ASC->AbilityActorInfo.Get(), WarriorGameplayTags::Player_Ability_TacticalTechnique, &Payload, *ASC))
		{
			// 끝나면 OnAbilityEnded에서 다음 칸
			return;
		}

		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);
		UE_LOG(LogProjectWarrior, Warning, TEXT("[TacticalMode] stock %d (%s) could not activate. Owner tags: %s"),
			ExecutingIndex, *GetNameSafe(Stock.Technique), *OwnedTags.ToStringSimple());
	}

	ExecutingTechniqueHandle = FGameplayAbilitySpecHandle();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPlayerGameplayAbility_TacticalMode::OnAbilityEnded(const FAbilityEndedData& AbilityEndedData)
{
	if (!ExecutingTechniqueHandle.IsValid() || AbilityEndedData.AbilitySpecHandle != ExecutingTechniqueHandle)
	{
		return;
	}

	ExecutingTechniqueHandle = FGameplayAbilitySpecHandle();

	// 피격 등으로 끊겼으면 남은 스톡도 취소
	if (AbilityEndedData.bWasCancelled)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[TacticalMode] stock %d was cancelled; remaining stocks dropped."), ExecutingIndex);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 끝나는 어빌리티의 콜백 안에서 바로 다음 어빌리티를 켜지 않도록 다음 틱으로 미룸
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive() && Step == ETacticalModeStep::Executing)
		{
			RunNextStock();
		}
	}));
}

#undef LOCTEXT_NAMESPACE
