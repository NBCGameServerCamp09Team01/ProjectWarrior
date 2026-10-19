// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioParameter.h"
#include "WarriorSoundTypes.generated.h"

class USoundAttenuation;
class USoundBase;

/**
 * 사운드 표(UWarriorSoundSet)의 효과음 한 칸.
 * Sound를 비워 두면 그 태그는 "소리 없음"으로 정한 것이다(부모 태그로 더 찾지 않는다).
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorSoundEntry
{
	GENERATED_BODY()

	//SoundWave·SoundCue·MetaSound 모두 쓸 수 있다. 무작위 재생 등은 SoundCue·MetaSound 안에서 만든다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.f;

	//위치가 있는 재생(PlaySoundAtLocation, SpawnSoundAttached)에서만 쓴다. 비우면 소리 에셋의 설정을 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation> Attenuation = nullptr;
};

/**
 * 사운드 표(UWarriorSoundSet)의 음악 한 칸.
 * Sound를 비워 두면 그 상황은 "무음"으로 정한 것이다(지금 곡을 페이드 아웃한다).
 */
USTRUCT(BlueprintType)
struct PROJECTWARRIOR_API FWarriorMusicEntry
{
	GENERATED_BODY()

	//SoundWave·SoundCue·MetaSound 모두 쓸 수 있다. 반복 재생은 소리 에셋의 Looping 또는 MetaSound 그래프에서 정한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0"))
	float Volume = 1.f;

	//이 곡으로 바뀔 때 페이드 인 시간. 1초는 자리 표시값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInSeconds = 1.f;

	//이 곡에서 다른 곡으로 바뀔 때 이 곡의 페이드 아웃 시간. 1초는 자리 표시값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutSeconds = 1.f;

	//재생 시작 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", Units = "s"))
	float StartTime = 0.f;

	//MetaSound 입력(이름 = 값 또는 트리거). 이 상황이 되면 곡에 넘긴다.
	//여러 상황이 같은 MetaSound를 쓰면 곡을 끊지 않고 이 값만 바꾼다 → MetaSound 그래프 안에서 구간 이동·구간 반복을 만든다.
	//일반 소리 파일이면 비워 둔다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<FAudioParameter> Parameters;
};
