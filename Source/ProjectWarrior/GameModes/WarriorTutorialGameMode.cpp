// Fill out your copyright notice in the Description page of Project Settings.

#include "WarriorTutorialGameMode.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/Components/Inventory/PlayerInventoryComponent.h"
#include "ProjectWarrior/Controllers/WarriorTutorialPlayerController.h"
#include "ProjectWarrior/DataAssets/DataAsset_Item.h"
#include "ProjectWarrior/PlayerStates/WarriorPlayerState.h"
#include "ProjectWarrior/Tutorial/DataAsset_TutorialSteps.h"
#include "ProjectWarrior/Tutorial/WarriorTutorialZone.h"

const TCHAR* AWarriorTutorialGameMode::ChapterOption = TEXT("Chapter");

AWarriorTutorialGameMode::AWarriorTutorialGameMode()
{
	PlayerControllerClass = AWarriorTutorialPlayerController::StaticClass();

	//InputUsed 단계의 입력을 읽는다. 일시정지 중에는 돌지 않는다
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

const FWarriorTutorialStep* AWarriorTutorialGameMode::GetCurrentStep() const
{
	return TutorialSteps ? TutorialSteps->FindStep(ChapterIndex, StepIndex) : nullptr;
}

void AWarriorTutorialGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	StartChapterIndex = FMath::Max(UGameplayStatics::GetIntOption(Options, ChapterOption, 0), 0);
	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] InitGame. startChapter=%d"), StartChapterIndex);
}

void AWarriorTutorialGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	//PossessedBy가 시작 데이터·계정 해금 태그를 붙인 뒤다(AWarriorPlayerCharacter::PossessedBy)
	if (NewPlayer && NewPlayer->IsPlayerController() && TutorialState == EWarriorTutorialState::Loading)
	{
		StartTutorial(NewPlayer->GetPawn());
	}
}

void AWarriorTutorialGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StepTimer);
	UnbindStepSignals();

	if (UAbilitySystemComponent* ASC = PlayerASC.Get())
	{
		ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
		ASC->AbilityFailedCallbacks.Remove(AbilityFailedHandle);
		ApplyAbilityBlocks(FGameplayTagContainer());
		ASC->RemoveLooseGameplayTags(AppliedSkillTags);
		AppliedSkillTags.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AWarriorTutorialGameMode::StartTutorial(APawn* InPlayerPawn)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InPlayerPawn);
	if (!TutorialSteps || TutorialSteps->GetChapters().IsEmpty() || !ASC)
	{
		UE_LOG(LogProjectWarrior, Error, TEXT("[Tutorial] Cannot start. steps=%s pawn=%s asc=%s"),
			*GetNameSafe(TutorialSteps), *GetNameSafe(InPlayerPawn), ASC ? TEXT("ok") : TEXT("none"));
		return;
	}

	PlayerPawn = InPlayerPawn;
	PlayerASC = ASC;

	for (TActorIterator<AWarriorTutorialZone> It(GetWorld()); It; ++It)
	{
		if (!It->GetZoneId().IsNone())
		{
			Zones.Add(It->GetZoneId(), *It);
		}
	}

	//튜토리얼에서만 잠긴 조작을 연다. 계정 사본·서버 값은 그대로이고, 레벨을 나가면 캐릭터와 함께 사라진다
	AppliedSkillTags = TutorialSteps->TemporarySkillTags;
	ASC->AddLooseGameplayTags(AppliedSkillTags);

	AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
	AbilityFailedHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &ThisClass::HandleAbilityFailed);

	const int32 FirstChapter = TutorialSteps->GetChapters().IsValidIndex(StartChapterIndex) ? StartChapterIndex : 0;
	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Started. chapter=%d zones=%d skillTags=%s"),
		FirstChapter, Zones.Num(), *AppliedSkillTags.ToStringSimple());

	EnterStep(FirstChapter, 0);
}

void AWarriorTutorialGameMode::EnterStep(int32 InChapterIndex, int32 InStepIndex)
{
	ChapterIndex = InChapterIndex;
	StepIndex = InStepIndex;
	const FWarriorTutorialStep* Step = GetCurrentStep();
	if (!Step)
	{
		FinishTutorial();
		return;
	}

	StepProgress = 0;
	StepFailures = 0;
	bHintShown = false;
	StepInputSeconds = 0.f;
	bStepInputWasActive = false;

	//처음부터 이 단계까지 푼 것만 쓸 수 있다
	FGameplayTagContainer DesiredBlocked = TutorialSteps->GetAllUnlockAbilityTags();
	DesiredBlocked.RemoveTags(TutorialSteps->GetUnlockedAbilityTagsUpTo(ChapterIndex, StepIndex));
	ApplyAbilityBlocks(DesiredBlocked);

	ActivateStepZone(*Step);
	GrantStepItems(*Step);

	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Step enter. chapter=%d step=%d id=%s goal=%s tag=%s"),
		ChapterIndex, StepIndex, *Step->StepId.ToString(), *UEnum::GetValueAsString(Step->Goal), *Step->GoalTag.ToString());

	SetTutorialState(EWarriorTutorialState::StepIntro);
	GetWorldTimerManager().SetTimer(StepTimer, this, &ThisClass::BeginStepActive, FMath::Max(StepIntroSeconds, 0.01f), false);
}

void AWarriorTutorialGameMode::BeginStepActive()
{
	const FWarriorTutorialStep* Step = GetCurrentStep();
	if (!Step)
	{
		return;
	}

	BindStepSignals(*Step);
	SetTutorialState(EWarriorTutorialState::StepActive);
}

void AWarriorTutorialGameMode::AddStepProgress()
{
	const FWarriorTutorialStep* Step = GetCurrentStep();
	if (TutorialState != EWarriorTutorialState::StepActive || !Step)
	{
		return;
	}

	++StepProgress;
	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Progress. id=%s %d/%d"), *Step->StepId.ToString(), StepProgress, Step->RequiredCount);

	if (StepProgress >= Step->RequiredCount)
	{
		CompleteStep();
		return;
	}
	OnTutorialChanged.Broadcast();
}

void AWarriorTutorialGameMode::CompleteStep()
{
	UnbindStepSignals();

	if (const FWarriorTutorialStep* Step = GetCurrentStep())
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Step done. chapter=%d step=%d id=%s failures=%d"),
			ChapterIndex, StepIndex, *Step->StepId.ToString(), StepFailures);
	}

	SetTutorialState(EWarriorTutorialState::StepDone);
	GetWorldTimerManager().SetTimer(StepTimer, this, &ThisClass::AdvanceStep, FMath::Max(StepDoneSeconds, 0.01f), false);
}

void AWarriorTutorialGameMode::AdvanceStep()
{
	GetWorldTimerManager().ClearTimer(StepTimer);
	UnbindStepSignals();

	const TArray<FWarriorTutorialChapter>& Chapters = TutorialSteps->GetChapters();
	int32 NextChapter = ChapterIndex;
	int32 NextStep = StepIndex + 1;
	if (!Chapters.IsValidIndex(NextChapter) || !Chapters[NextChapter].Steps.IsValidIndex(NextStep))
	{
		++NextChapter;
		NextStep = 0;
	}

	if (!Chapters.IsValidIndex(NextChapter))
	{
		FinishTutorial();
		return;
	}
	EnterStep(NextChapter, NextStep);
}

bool AWarriorTutorialGameMode::CanSkipCurrentStep() const
{
	return TutorialState == EWarriorTutorialState::StepIntro
		|| TutorialState == EWarriorTutorialState::StepActive
		|| TutorialState == EWarriorTutorialState::StepDone;
}

void AWarriorTutorialGameMode::SkipCurrentStep()
{
	if (!CanSkipCurrentStep())
	{
		return;
	}

	if (const FWarriorTutorialStep* Step = GetCurrentStep())
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Step skipped. chapter=%d step=%d id=%s"), ChapterIndex, StepIndex, *Step->StepId.ToString());
	}
	AdvanceStep();
}

void AWarriorTutorialGameMode::FinishTutorial()
{
	GetWorldTimerManager().ClearTimer(StepTimer);
	UnbindStepSignals();

	if (AWarriorTutorialZone* Zone = ActiveZone.Get())
	{
		Zone->DeactivateZone();
	}
	ActiveZone.Reset();

	//다 배웠으니 모두 푼다(완료 화면 뒤에는 레벨을 떠난다)
	ApplyAbilityBlocks(FGameplayTagContainer());

	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Finished."));
	SetTutorialState(EWarriorTutorialState::Finished);
}

void AWarriorTutorialGameMode::SetTutorialState(EWarriorTutorialState InState)
{
	TutorialState = InState;
	OnTutorialChanged.Broadcast();
}

void AWarriorTutorialGameMode::ApplyAbilityBlocks(const FGameplayTagContainer& InDesiredBlocked)
{
	UAbilitySystemComponent* ASC = PlayerASC.Get();
	if (!ASC)
	{
		AppliedBlockedTags.Reset();
		return;
	}

	FGameplayTagContainer ToBlock;
	for (const FGameplayTag& Tag : InDesiredBlocked)
	{
		if (!AppliedBlockedTags.HasTagExact(Tag))
		{
			ToBlock.AddTag(Tag);
		}
	}

	FGameplayTagContainer ToUnblock;
	for (const FGameplayTag& Tag : AppliedBlockedTags)
	{
		if (!InDesiredBlocked.HasTagExact(Tag))
		{
			ToUnblock.AddTag(Tag);
		}
	}

	if (!ToBlock.IsEmpty())
	{
		ASC->BlockAbilitiesWithTags(ToBlock);
		UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Block %s"), *ToBlock.ToStringSimple());
	}
	if (!ToUnblock.IsEmpty())
	{
		ASC->UnBlockAbilitiesWithTags(ToUnblock);
		UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] UnBlock %s"), *ToUnblock.ToStringSimple());
	}
	AppliedBlockedTags = InDesiredBlocked;
}

void AWarriorTutorialGameMode::ActivateStepZone(const FWarriorTutorialStep& InStep)
{
	if (InStep.ZoneId.IsNone())
	{
		return;
	}

	AWarriorTutorialZone* NextZone = Zones.FindRef(InStep.ZoneId);
	if (!NextZone)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] Zone %s is not in the level."), *InStep.ZoneId.ToString());
		return;
	}

	//다른 구역으로 가면 지난 구역의 상대를 치운다
	if (AWarriorTutorialZone* PreviousZone = ActiveZone.Get())
	{
		if (PreviousZone != NextZone)
		{
			PreviousZone->DeactivateZone();
		}
	}

	ActiveZone = NextZone;
	NextZone->ActivateZone(PlayerPawn.Get());
}

void AWarriorTutorialGameMode::GrantStepItems(const FWarriorTutorialStep& InStep)
{
	UPlayerInventoryComponent* Inventory = GetPlayerInventory();
	StepItemCounts.Reset();
	if (!Inventory)
	{
		return;
	}

	for (const FWarriorTutorialItemGrant& Grant : InStep.GrantItems)
	{
		if (!Grant.Item)
		{
			continue;
		}
		if (!Inventory->AddItem(Grant.Item, Grant.Count))
		{
			UE_LOG(LogProjectWarrior, Warning, TEXT("[Tutorial] Failed to grant %s x%d."), *GetNameSafe(Grant.Item), Grant.Count);
		}
		StepItemCounts.Add(Grant.Item, Inventory->GetItemCount(Grant.Item));
	}
}

UPlayerInventoryComponent* AWarriorTutorialGameMode::GetPlayerInventory() const
{
	const APawn* Pawn = PlayerPawn.Get();
	const AWarriorPlayerState* WarriorPlayerState = Pawn ? Pawn->GetPlayerState<AWarriorPlayerState>() : nullptr;
	return WarriorPlayerState ? WarriorPlayerState->GetPlayerInventoryComponent() : nullptr;
}

bool AWarriorTutorialGameMode::HasRequiredOwnedTag(const FWarriorTutorialStep& InStep) const
{
	if (!InStep.RequiredOwnedTag.IsValid())
	{
		return true;
	}
	const UAbilitySystemComponent* ASC = PlayerASC.Get();
	return ASC && ASC->HasMatchingGameplayTag(InStep.RequiredOwnedTag);
}

//~ Begin 신호

void AWarriorTutorialGameMode::BindStepSignals(const FWarriorTutorialStep& InStep)
{
	UnbindStepSignals();

	UAbilitySystemComponent* ASC = PlayerASC.Get();
	if (!ASC)
	{
		return;
	}

	BoundGoal = InStep.Goal;
	switch (InStep.Goal)
	{
	case EWarriorTutorialGoal::GameplayEvent:
		BoundGoalTag = InStep.GoalTag;
		GoalSignalHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(BoundGoalTag).AddUObject(this, &ThisClass::HandleGoalEvent);
		break;

	case EWarriorTutorialGoal::TagAdded:
		BoundGoalTag = InStep.GoalTag;
		GoalSignalHandle = ASC->RegisterGameplayTagEvent(BoundGoalTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleGoalTagChanged);
		break;

	case EWarriorTutorialGoal::ReachZone:
		if (AWarriorTutorialZone* Zone = Zones.FindRef(InStep.ZoneId))
		{
			ZoneReachedHandle = Zone->OnZoneReached.AddUObject(this, &ThisClass::HandleZoneReached);
		}
		break;

	case EWarriorTutorialGoal::ItemUsed:
		if (UPlayerInventoryComponent* Inventory = GetPlayerInventory())
		{
			Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
			bInventoryBound = true;
		}
		break;

	default:
		//AbilityActivated는 시작 때 묶은 콜백이, InputUsed는 Tick이 본다
		break;
	}

	if (InStep.FailureEventTag.IsValid())
	{
		BoundFailureTag = InStep.FailureEventTag;
		FailureSignalHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(BoundFailureTag).AddUObject(this, &ThisClass::HandleFailureEvent);
	}
}

void AWarriorTutorialGameMode::UnbindStepSignals()
{
	UAbilitySystemComponent* ASC = PlayerASC.Get();
	if (ASC)
	{
		if (GoalSignalHandle.IsValid())
		{
			if (BoundGoal == EWarriorTutorialGoal::GameplayEvent)
			{
				if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(BoundGoalTag))
				{
					Delegate->Remove(GoalSignalHandle);
				}
			}
			else if (BoundGoal == EWarriorTutorialGoal::TagAdded)
			{
				ASC->RegisterGameplayTagEvent(BoundGoalTag, EGameplayTagEventType::NewOrRemoved).Remove(GoalSignalHandle);
			}
		}
		if (FailureSignalHandle.IsValid())
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(BoundFailureTag))
			{
				Delegate->Remove(FailureSignalHandle);
			}
		}
	}

	if (ZoneReachedHandle.IsValid())
	{
		for (const TPair<FName, TObjectPtr<AWarriorTutorialZone>>& Pair : Zones)
		{
			if (Pair.Value)
			{
				Pair.Value->OnZoneReached.Remove(ZoneReachedHandle);
			}
		}
	}

	if (bInventoryBound)
	{
		if (UPlayerInventoryComponent* Inventory = GetPlayerInventory())
		{
			Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		}
		bInventoryBound = false;
	}

	GoalSignalHandle.Reset();
	FailureSignalHandle.Reset();
	ZoneReachedHandle.Reset();
	BoundGoalTag = FGameplayTag();
	BoundFailureTag = FGameplayTag();
}

void AWarriorTutorialGameMode::HandleAbilityActivated(UGameplayAbility* InAbility)
{
	const FWarriorTutorialStep* Step = GetCurrentStep();
	if (!InAbility || !Step || Step->Goal != EWarriorTutorialGoal::AbilityActivated)
	{
		return;
	}

	//자기 태그(부모 포함)가 GoalTag면 센다. 예: GoalTag Player.Ability.Attack.Katana.Light.4
	if (InAbility->GetAssetTags().HasTag(Step->GoalTag) && HasRequiredOwnedTag(*Step))
	{
		AddStepProgress();
	}
}

void AWarriorTutorialGameMode::HandleAbilityFailed(const UGameplayAbility* InAbility, const FGameplayTagContainer& InFailureTags)
{
	const UAbilitySystemComponent* ASC = PlayerASC.Get();
	if (!InAbility || !ASC || !ASC->AreAbilityTagsBlocked(InAbility->GetAssetTags()))
	{
		return;
	}

	const double Now = GetWorld()->GetRealTimeSeconds();
	if (LastBlockedNoticeTime >= 0.0 && Now - LastBlockedNoticeTime < BlockedNoticeInterval)
	{
		return;
	}
	LastBlockedNoticeTime = Now;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Blocked ability %s"), *InAbility->GetAssetTags().ToStringSimple());
	OnAbilityBlocked.Broadcast();
}

void AWarriorTutorialGameMode::HandleGoalEvent(const FGameplayEventData* InPayload)
{
	if (const FWarriorTutorialStep* Step = GetCurrentStep())
	{
		if (HasRequiredOwnedTag(*Step))
		{
			AddStepProgress();
		}
	}
}

void AWarriorTutorialGameMode::HandleFailureEvent(const FGameplayEventData* InPayload)
{
	const FWarriorTutorialStep* Step = GetCurrentStep();
	if (TutorialState != EWarriorTutorialState::StepActive || !Step)
	{
		return;
	}

	++StepFailures;
	if (!bHintShown && Step->HintAfterFailures > 0 && StepFailures >= Step->HintAfterFailures)
	{
		bHintShown = true;
		UE_LOG(LogProjectWarrior, Log, TEXT("[Tutorial] Hint shown. id=%s failures=%d"), *Step->StepId.ToString(), StepFailures);
		OnTutorialChanged.Broadcast();
	}
}

void AWarriorTutorialGameMode::HandleGoalTagChanged(const FGameplayTag InTag, int32 InNewCount)
{
	//새로 붙었을 때만 센다(NewOrRemoved는 0 ↔ 1 이상일 때 온다)
	if (InNewCount > 0)
	{
		if (const FWarriorTutorialStep* Step = GetCurrentStep())
		{
			if (HasRequiredOwnedTag(*Step))
			{
				AddStepProgress();
			}
		}
	}
}

void AWarriorTutorialGameMode::HandleZoneReached(AWarriorTutorialZone* InZone)
{
	AddStepProgress();
}

void AWarriorTutorialGameMode::HandleInventoryChanged()
{
	UPlayerInventoryComponent* Inventory = GetPlayerInventory();
	if (!Inventory)
	{
		return;
	}

	//준 아이템이 줄었으면 쓴 것으로 본다
	for (TPair<const UDataAsset_Item*, int32>& Pair : StepItemCounts)
	{
		const int32 Count = Inventory->GetItemCount(Pair.Key);
		if (Count < Pair.Value)
		{
			Pair.Value = Count;
			AddStepProgress();
			return;
		}
		Pair.Value = Count;
	}
}

//~ End 신호

void AWarriorTutorialGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FWarriorTutorialStep* Step = GetCurrentStep();
	if (TutorialState != EWarriorTutorialState::StepActive || !Step || Step->Goal != EWarriorTutorialGoal::InputUsed || !Step->KeyAction)
	{
		return;
	}

	//입력에 묶지 않고 값만 읽는다(묶으면 캐릭터의 같은 입력과 겹칠 수 있다)
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	const UEnhancedPlayerInput* PlayerInput = InputSubsystem ? InputSubsystem->GetPlayerInput() : nullptr;
	if (!PlayerInput)
	{
		return;
	}

	const bool bActive = PlayerInput->GetActionValue(Step->KeyAction).GetMagnitude() > KINDA_SMALL_NUMBER;
	if (Step->RequiredInputSeconds <= 0.f)
	{
		//누른 순간 한 번
		if (bActive && !bStepInputWasActive)
		{
			AddStepProgress();
		}
	}
	else if (bActive)
	{
		StepInputSeconds += DeltaSeconds;
		if (StepInputSeconds >= Step->RequiredInputSeconds)
		{
			StepInputSeconds = 0.f;
			AddStepProgress();
		}
	}
	bStepInputWasActive = bActive;
}
