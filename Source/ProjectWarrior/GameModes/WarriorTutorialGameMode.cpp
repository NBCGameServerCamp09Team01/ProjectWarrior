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
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAttributeSet.h"
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
	WaitForAbilityTags.AddTag(WarriorGameplayTags::Player_Ability_Finisher);

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
	ClearStepTimer();
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
	SetStepTimer(StepIntroSeconds, [this]() { BeginStepActive(); });
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

		//다음 틱에 끈다. 완료 신호는 어빌리티 안(예: 블레이드 모드 PerformSlash)에서 오므로 바로 끄면 남은 코드가 자세를 다시 켠다
		if (!Step->CancelAbilityTagsOnComplete.IsEmpty() && PlayerASC.IsValid())
		{
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
				[WeakASC = PlayerASC, CancelTags = Step->CancelAbilityTagsOnComplete]()
				{
					if (UAbilitySystemComponent* ASC = WeakASC.Get())
					{
						ASC->CancelAbilities(&CancelTags);
					}
				}));
		}
	}

	SetTutorialState(EWarriorTutorialState::StepDone);
	SetStepTimer(StepDoneSeconds, [this]() { HandleStepDoneElapsed(); });
}

void AWarriorTutorialGameMode::HandleStepDoneElapsed()
{
	if (IsPlayerAbilityBusy())
	{
		SetStepTimer(0.25f, [this]() { HandleStepDoneElapsed(); });
		return;
	}
	AdvanceStep();
}

bool AWarriorTutorialGameMode::IsPlayerAbilityBusy() const
{
	const UAbilitySystemComponent* ASC = PlayerASC.Get();
	if (!ASC || WaitForAbilityTags.IsEmpty())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && Spec.Ability->GetAssetTags().HasAny(WaitForAbilityTags))
		{
			return true;
		}
	}
	return false;
}

void AWarriorTutorialGameMode::AdvanceStep()
{
	ClearStepTimer();
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
	ClearStepTimer();
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
		//하위 태그 이벤트도 받는다(예: Player.Event.SwitchTarget → .Left/.Right). GenericGameplayEventCallbacks는 태그가 정확히 같을 때만 온다
		GoalSignalHandle = ASC->AddGameplayEventTagContainerDelegate(FGameplayTagContainer(BoundGoalTag),
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleGoalEvent));
		break;

	case EWarriorTutorialGoal::TagAdded:
		BoundGoalTag = InStep.GoalTag;
		GoalSignalHandle = ASC->RegisterGameplayTagEvent(BoundGoalTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleGoalTagChanged);
		break;

	case EWarriorTutorialGoal::ReachZone:
		if (AWarriorTutorialZone* Zone = Zones.FindRef(InStep.ZoneId))
		{
			ZoneReachedHandle = Zone->OnZoneReached.AddUObject(this, &ThisClass::HandleZoneReached);
			Zone->SetGoalMarkerVisible(true);
		}
		break;

	case EWarriorTutorialGoal::ZoneEnemyEvent:
		BoundGoalTag = InStep.GoalTag;
		if (AWarriorTutorialZone* Zone = Zones.FindRef(InStep.ZoneId))
		{
			ZoneEnemyEventHandle = Zone->OnEnemyEvent.AddUObject(this, &ThisClass::HandleZoneEnemyEvent);
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
		FailureSignalHandle = ASC->AddGameplayEventTagContainerDelegate(FGameplayTagContainer(BoundFailureTag),
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleFailureEvent));
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
				ASC->RemoveGameplayEventTagContainerDelegate(FGameplayTagContainer(BoundGoalTag), GoalSignalHandle);
			}
			else if (BoundGoal == EWarriorTutorialGoal::TagAdded)
			{
				ASC->RegisterGameplayTagEvent(BoundGoalTag, EGameplayTagEventType::NewOrRemoved).Remove(GoalSignalHandle);
			}
		}
		if (FailureSignalHandle.IsValid())
		{
			ASC->RemoveGameplayEventTagContainerDelegate(FGameplayTagContainer(BoundFailureTag), FailureSignalHandle);
		}
	}

	if (ZoneReachedHandle.IsValid())
	{
		for (const TPair<FName, TObjectPtr<AWarriorTutorialZone>>& Pair : Zones)
		{
			if (Pair.Value)
			{
				Pair.Value->OnZoneReached.Remove(ZoneReachedHandle);
				Pair.Value->SetGoalMarkerVisible(false);
			}
		}
	}

	if (ZoneEnemyEventHandle.IsValid())
	{
		for (const TPair<FName, TObjectPtr<AWarriorTutorialZone>>& Pair : Zones)
		{
			if (Pair.Value)
			{
				Pair.Value->OnEnemyEvent.Remove(ZoneEnemyEventHandle);
			}
		}
		ZoneEnemyEventHandle.Reset();
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

void AWarriorTutorialGameMode::HandleGoalEvent(FGameplayTag InEventTag, const FGameplayEventData* InPayload)
{
	if (const FWarriorTutorialStep* Step = GetCurrentStep())
	{
		if (HasRequiredOwnedTag(*Step))
		{
			AddStepProgress();
		}
	}
}

void AWarriorTutorialGameMode::HandleFailureEvent(FGameplayTag InEventTag, const FGameplayEventData* InPayload)
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

void AWarriorTutorialGameMode::HandleZoneEnemyEvent(AWarriorTutorialZone* InZone, FGameplayTag InEventTag)
{
	//하위 태그도 센다(예: Shared.Event.Stagger.*)
	if (BoundGoal == EWarriorTutorialGoal::ZoneEnemyEvent && BoundGoalTag.IsValid() && InEventTag.MatchesTag(BoundGoalTag))
	{
		AddStepProgress();
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

void AWarriorTutorialGameMode::SetStepTimer(float InRealSeconds, TFunction<void()>&& InCallback)
{
	StepTimerRemaining = FMath::Max(InRealSeconds, 0.01f);
	StepTimerCallback = MoveTemp(InCallback);
}

void AWarriorTutorialGameMode::ClearStepTimer()
{
	StepTimerCallback.Reset();
	StepTimerRemaining = 0.f;
}

void AWarriorTutorialGameMode::TickStepTimer(float InDeltaSeconds)
{
	if (!StepTimerCallback)
	{
		return;
	}

	//DeltaSeconds는 세상 시간 배율이 곱해져 있다. 나눠서 실제 시간으로 센다
	const AWorldSettings* WorldSettings = GetWorldSettings();
	const float TimeDilation = WorldSettings ? WorldSettings->GetEffectiveTimeDilation() : 1.f;
	StepTimerRemaining -= InDeltaSeconds / FMath::Max(TimeDilation, KINDA_SMALL_NUMBER);
	if (StepTimerRemaining > 0.f)
	{
		return;
	}

	//콜백이 새 타이머를 걸 수 있으므로 먼저 비운다
	TFunction<void()> Callback = MoveTemp(StepTimerCallback);
	ClearStepTimer();
	Callback();
}

void AWarriorTutorialGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TickStepTimer(DeltaSeconds);

	//튜토리얼에는 스태미나를 회복할 수단이 없다. 블레이드 모드 유지·막기로 바닥나지 않게 늘 채워 둔다
	if (bKeepPlayerStaminaFull && PlayerASC.IsValid())
	{
		const float MaxStamina = PlayerASC->GetNumericAttribute(UWarriorAttributeSet::GetMaxStaminaAttribute());
		if (PlayerASC->GetNumericAttribute(UWarriorAttributeSet::GetCurrentStaminaAttribute()) < MaxStamina)
		{
			PlayerASC->SetNumericAttributeBase(UWarriorAttributeSet::GetCurrentStaminaAttribute(), MaxStamina);
		}
	}

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
