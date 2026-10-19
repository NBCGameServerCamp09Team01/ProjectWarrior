// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WarriorSoundTypes.h"
#include "WarriorSoundSet.generated.h"

/**
 * 사운드 표. 태그마다 어떤 소리를 쓸지 정한다. 소리를 바꿔 끼울 때는 이 에셋만 고친다.
 * 프로젝트 설정 > Game > Warrior Sound의 Sound Set에 지정한 표 하나를 게임 전체가 쓴다.
 *
 * 찾는 규칙: 정확한 태그 → 부모 태그 순서로 찾는다.
 *   예) Sound.Player.Attack.Swing.Fire가 없으면 Sound.Player.Attack.Swing을 쓴다.
 * 칸이 있으면 Sound가 비어 있어도 거기서 멈춘다(그 태그는 소리 없음으로 정한 것).
 */
UCLASS(BlueprintType)
class PROJECTWARRIOR_API UWarriorSoundSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//InTag의 효과음 칸을 찾는다. OutMatchedTag에는 실제로 찾은 태그(부모일 수 있음)를 돌려준다
	const FWarriorSoundEntry* FindSound(const FGameplayTag& InTag, FGameplayTag* OutMatchedTag = nullptr) const;

	//InTag의 음악 칸을 찾는다. OutMatchedTag에는 실제로 찾은 태그(부모일 수 있음)를 돌려준다
	const FWarriorMusicEntry* FindMusic(const FGameplayTag& InTag, FGameplayTag* OutMatchedTag = nullptr) const;

protected:
	//효과음 (Sound.*)
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (Categories = "Sound", ForceInlineRow))
	TMap<FGameplayTag, FWarriorSoundEntry> Sounds;

	//음악 상황 (Music.*)
	UPROPERTY(EditAnywhere, Category = "Music", meta = (Categories = "Music", ForceInlineRow))
	TMap<FGameplayTag, FWarriorMusicEntry> Music;
};
