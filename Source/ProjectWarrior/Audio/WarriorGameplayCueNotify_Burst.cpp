// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGameplayCueNotify_Burst.h"
#include "GameFramework/Actor.h"
#include "GameplayCueNotifyTypes.h"
#include "WarriorSoundSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#endif

#define LOCTEXT_NAMESPACE "WarriorGameplayCueNotify_Burst"

bool UWarriorGameplayCueNotify_Burst::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	//이펙트·카메라 흔들림 등은 엔진 GCN Burst가 처리한다.
	const bool bResult = Super::OnExecute_Implementation(Target, Parameters);

	PlayTableSound(Target, Parameters);
	return bResult;
}

void UWarriorGameplayCueNotify_Burst::PlayTableSound(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!SoundTag.IsValid())
	{
		return;
	}

	UWorld* World = Target ? Target->GetWorld() : GetWorld();
	if (!World)
	{
		return;
	}

	//엔진 Burst Sounds와 같은 규칙: 기본 생성 조건을 보고, 기본 배치(위치·소켓·부착)로 낸다.
	FGameplayCueNotify_SpawnContext SpawnContext(World, Target, Parameters);
	if (!DefaultSpawnCondition.ShouldSpawn(SpawnContext))
	{
		return;
	}

	FTransform SpawnTransform;
	if (!DefaultPlacementInfo.FindSpawnTransform(SpawnContext, SpawnTransform))
	{
		return;
	}

	if (SpawnContext.TargetComponent && DefaultPlacementInfo.AttachPolicy == EGameplayCueNotify_AttachPolicy::AttachToTarget)
	{
		UWarriorSoundSubsystem::SpawnSoundAttached(SoundTag, SpawnContext.TargetComponent, DefaultPlacementInfo.SocketName);
	}
	else
	{
		UWarriorSoundSubsystem::PlaySoundAtLocation(World, SoundTag, SpawnTransform.GetLocation());
	}
}

#if WITH_EDITOR
EDataValidationResult UWarriorGameplayCueNotify_Burst::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);

	//Sound Tag와 Burst Sounds가 함께 있으면 같은 소리가 두 번 난다.
	//Burst Sounds는 엔진 구조체 안에서 protected이고 구조체가 모듈 밖으로 내보내지지 않아,
	//클래스 리플렉션(BurstEffects → BurstSounds)으로 개수만 본다. 이름이 바뀌면 검사를 건너뛴다.
	if (SoundTag.IsValid())
	{
		const FStructProperty* EffectsProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("BurstEffects"));
		const FArrayProperty* BurstSoundsProperty = EffectsProperty ? FindFProperty<FArrayProperty>(EffectsProperty->Struct, TEXT("BurstSounds")) : nullptr;
		if (BurstSoundsProperty)
		{
			const void* EffectsValue = EffectsProperty->ContainerPtrToValuePtr<void>(this);
			FScriptArrayHelper BurstSounds(BurstSoundsProperty, BurstSoundsProperty->ContainerPtrToValuePtr<void>(EffectsValue));
			if (BurstSounds.Num() > 0)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("SoundTagWithBurstSounds", "{0}: Sound Tag({1})와 Burst Sounds가 함께 있어 같은 소리가 두 번 납니다. Burst Sounds를 비우세요."),
					FText::FromString(GetName()), FText::FromString(SoundTag.ToString())));
			}
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
