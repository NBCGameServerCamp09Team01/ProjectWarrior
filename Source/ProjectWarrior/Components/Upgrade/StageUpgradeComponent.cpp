#include "StageUpgradeComponent.h"
#include "GameFramework/PlayerState.h"
#include "ProjectWarrior/DataAssets/DataAsset_Upgrade.h"
#include "ProjectWarrior/WarriorFunctionLibrary.h"
#include "ProjectWarrior/WarriorGamePlayTags.h"
#include "ProjectWarrior/AbilitySystem/WarriorAbilitySystemComponent.h"

UStageUpgradeComponent::UStageUpgradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStageUpgradeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		PS->OnPawnSet.AddUniqueDynamic(this, &ThisClass::HandlePawnSet);
	}
}

void UStageUpgradeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		PS->OnPawnSet.RemoveDynamic(this, &ThisClass::HandlePawnSet);
	}
	Super::EndPlay(EndPlayReason);
}

bool UStageUpgradeComponent::IncreaseLevel(const UDataAsset_Upgrade* InUpgrade)
{
	if (!InUpgrade || !InUpgrade->UpgradeId.IsValid() || !InUpgrade->UpgradeEffect || IsMaxLevel(InUpgrade))
	{
		return false;
	}

	int32& Level = Levels.FindOrAdd(InUpgrade->UpgradeId);
	++Level;
	UpgradeAssets.Add(InUpgrade->UpgradeId, InUpgrade);

	ApplyUpgrade(InUpgrade, true);

	OnUpgradeChanged.Broadcast(InUpgrade->UpgradeId, Level);
	return true;
}

bool UStageUpgradeComponent::IsMaxLevel(const UDataAsset_Upgrade* InUpgrade) const
{
	return InUpgrade && GetLevel(InUpgrade) >= InUpgrade->GetMaxLevel();
}

int32 UStageUpgradeComponent::GetLevel(const UDataAsset_Upgrade* InUpgrade) const
{
	const int32* Level = InUpgrade ? Levels.Find(InUpgrade->UpgradeId) : nullptr;
	return Level ? *Level : 0;
}

void UStageUpgradeComponent::ResetAll()
{
	TArray<FGameplayTag> Ids;
	ActiveHandles.GetKeys(Ids);
	for (const FGameplayTag& Id : Ids)
	{
		RemoveUpgradeEffect(Id);
	}

	Levels.Reset();
	UpgradeAssets.Reset();
	ActiveHandles.Reset();

	OnUpgradesReset.Broadcast();
}

void UStageUpgradeComponent::ApplyUpgrade(const UDataAsset_Upgrade* InUpgrade, bool bRestoreIncreasedAmount)
{
	UWarriorAbilitySystemComponent* ASC = GetOwningASC();
	if (!ASC)
	{
		return;  // 폰이 아직 없으면 레벨만 저장. HandlePawnSet에서 적용된다
	}

	// 최대치 강화(MaxHealth 등)면 변경 전 값을 기록해 두고, 늘어난 만큼 현재값을 채운다
	const bool bShouldRestore = bRestoreIncreasedAmount
		&& InUpgrade->IncreasedAttribute.IsValid() && InUpgrade->RestoreAttribute.IsValid();
	const float OldMax = bShouldRestore ? ASC->GetNumericAttribute(InUpgrade->IncreasedAttribute) : 0.f;

	const int32 Level = Levels.FindRef(InUpgrade->UpgradeId);
	const float Value = InUpgrade->GetValueAtLevel(Level);

	// 이미 걸려 있으면 값만 갱신한다.
	// 지웠다 다시 걸면 그 사이 최대치가 기본값으로 떨어져 PostAttributeChange가 현재값을 잘라버린다.
	const FActiveGameplayEffectHandle* ExistingHandle = ActiveHandles.Find(InUpgrade->UpgradeId);
	if (ExistingHandle && ExistingHandle->IsValid())
	{
		ASC->UpdateActiveGameplayEffectSetByCallerMagnitude(*ExistingHandle, WarriorGameplayTags::Shared_SetByCaller_Upgrade, Value);
	}
	else
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(InUpgrade);

		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(InUpgrade->UpgradeEffect, 1.f, Context);
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(WarriorGameplayTags::Shared_SetByCaller_Upgrade, Value);
		ActiveHandles.Add(InUpgrade->UpgradeId, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	}

	if (bShouldRestore)
	{
		// 실제로 늘어난 양 (레벨 누적값 차이). 예: Lv1(+100) → Lv2(+110) 이면 10
		const float Delta = ASC->GetNumericAttribute(InUpgrade->IncreasedAttribute) - OldMax;
		if (Delta > 0.f)
		{
			// BaseValue만 바뀌므로 UI 갱신은 PostAttributeChange가 맡는다
			ASC->ApplyModToAttribute(InUpgrade->RestoreAttribute, EGameplayModOp::AddBase, Delta);
		}
	}
}

void UStageUpgradeComponent::RemoveUpgradeEffect(FGameplayTag InUpgradeId)
{
	FActiveGameplayEffectHandle Handle;
	if (ActiveHandles.RemoveAndCopyValue(InUpgradeId, Handle) && Handle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UStageUpgradeComponent::HandlePawnSet(APlayerState* InPlayer, APawn* InNewPawn, APawn* InOldPawn)
{
	ActiveHandles.Reset();  // 이전 폰의 핸들은 폰과 함께 사라졌으므로 버린다
	if (!InNewPawn)
	{
		return;
	}
	for (const TPair<FGameplayTag, TObjectPtr<const UDataAsset_Upgrade>>& Pair : UpgradeAssets)
	{
		ApplyUpgrade(Pair.Value, true);
	}
}

UWarriorAbilitySystemComponent* UStageUpgradeComponent::GetOwningASC() const
{
	const APlayerState* PS = GetOwner<APlayerState>();
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	return Pawn ? UWarriorFunctionLibrary::NativeGetWarriorASCFromActor(Pawn) : nullptr;
}