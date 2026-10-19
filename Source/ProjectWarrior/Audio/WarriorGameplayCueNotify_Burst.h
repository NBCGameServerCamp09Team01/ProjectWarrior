// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "GameplayTagContainer.h"
#include "WarriorGameplayCueNotify_Burst.generated.h"

/**
 * GCN Warrior Burst: 전투 피드백용 GameplayCue 노티파이(엔진 GCN Burst 확장).
 * 큐 에셋을 만들 때 부모 클래스로 고른다(GameplayCue 태그는 GameplayCue.* 로 시작).
 *
 * - 이펙트·카메라 흔들림·진동·데칼: 엔진 기능 그대로(GCN Effects > Burst Effects).
 * - 소리: 사운드 표(UWarriorSoundSet)에서. Sound Tag의 소리를 큐 위치에서 낸다 → 소리를 바꿀 때는 표만 고친다.
 * - 위치·소켓·부착과 생성 조건: GCN Defaults(Default Placement Info, Default Spawn Condition)를 이펙트와 똑같이 따른다.
 *
 * 주의
 * - Burst Effects의 Burst Sounds는 비워 둔다. 넣으면 같은 소리가 두 번 난다(데이터 검사에서 경고).
 * - 한 번 나는 소리만 쓴다. 효과가 붙어 있는 동안 나는 반복 소리(버프 등)는 Looping 큐가 필요하다(아직 없음).
 * 언제 GameplayCue를 쓰는지는 사운드 시스템 명세(SoundSystem_20261001) 1장.
 */
UCLASS(Blueprintable, Category = "GameplayCueNotify", meta = (DisplayName = "GCN Warrior Burst", ShortTooltip = "GCN Burst에 사운드 표의 소리를 더한 일회성 GameplayCue."))
class PROJECTWARRIOR_API UWarriorGameplayCueNotify_Burst : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	//~ Begin UGameplayCueNotify_Static Interface.
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
	//~ End UGameplayCueNotify_Static Interface

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	//낼 소리의 태그(사운드 표의 Sound.* 칸). 비우면 소리 없이 이펙트만 낸다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior Sound", meta = (Categories = "Sound"))
	FGameplayTag SoundTag;

private:
	//Default Spawn Condition·Default Placement Info를 따라 표의 소리를 낸다(엔진 Burst Sounds와 같은 규칙)
	void PlayTableSound(AActor* Target, const FGameplayCueParameters& Parameters) const;
};
