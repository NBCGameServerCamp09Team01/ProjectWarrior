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

	ApplyUpgrade(InUpgrade);

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

void UStageUpgradeComponent::ApplyUpgrade(const UDataAsset_Upgrade* InUpgrade)
{
	// 누적 값 1개로 덮어쓰기: 기존 GE를 지우고 현재 레벨의 누적 수치로 다시 건다
	RemoveUpgradeEffect(InUpgrade->UpgradeId);

	UWarriorAbilitySystemComponent* ASC = GetOwningASC();
	if (!ASC)
	{
		return;  // 폰이 아직 없으면 레벨만 저장. HandlePawnSet에서 적용된다
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(InUpgrade);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(InUpgrade->UpgradeEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	const int32 Level = Levels.FindRef(InUpgrade->UpgradeId);
	Spec.Data->SetSetByCallerMagnitude(WarriorGameplayTags::Shared_SetByCaller_Upgrade, InUpgrade->GetValueAtLevel(Level));

	ActiveHandles.Add(InUpgrade->UpgradeId, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
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
		ApplyUpgrade(Pair.Value);
	}
}

UWarriorAbilitySystemComponent* UStageUpgradeComponent::GetOwningASC() const
{
	const APlayerState* PS = GetOwner<APlayerState>();
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	return Pawn ? UWarriorFunctionLibrary::NativeGetWarriorASCFromActor(Pawn) : nullptr;
}