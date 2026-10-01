// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "WarriorSoundSubsystem.generated.h"

class UAudioComponent;
class USceneComponent;
class UUserWidget;
class UWarriorSoundSet;
struct FWarriorSoundEntry;

/**
 * 게임 전체의 소리 재생 창구 (GameInstance 서브시스템).
 * 코드·블루프린트·노티파이는 "무엇이 났는지"를 태그(WarriorSoundTags)로만 알리고,
 * "어떤 소리를 낼지"는 사운드 표(UWarriorSoundSet, 프로젝트 설정 > Game > Warrior Sound)가 정한다.
 * 소리를 바꿔 끼울 때는 표만 고친다.
 *
 * - 효과음: PlaySound2D / PlaySoundAtLocation / SpawnSoundAttached (정적 함수, 한 줄 호출)
 * - 음악: SetMusicState(상황 태그). 곡은 한 번에 하나만 재생하고, 레벨이 바뀌어도 끊기지 않는다.
 * - 버튼: ApplyButtonSounds(위젯)로 위젯 안의 모든 버튼에 클릭·호버 소리를 넣는다.
 * 표가 지정되지 않았거나 칸이 없으면 소리만 나지 않고 게임은 그대로 동작한다.
 */
UCLASS()
class PROJECTWARRIOR_API UWarriorSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWarriorSoundSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin 효과음
	//화면에 붙은 소리(UI 등)를 한 번 재생한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlaySound2D(const UObject* WorldContextObject, FGameplayTag SoundTag);

	//월드의 한 지점에서 소리를 한 번 재생한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlaySoundAtLocation(const UObject* WorldContextObject, FGameplayTag SoundTag, FVector Location);

	//컴포넌트에 붙여 따라다니는 소리를 재생한다(예: 날아가는 화살). 붙은 컴포넌트가 사라지면 멈춘다.
	//멈춰야 할 때(명중 등) 돌려받은 컴포넌트의 Stop을 부른다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Sound")
	static UAudioComponent* SpawnSoundAttached(FGameplayTag SoundTag, USceneComponent* AttachToComponent, FName AttachPointName = NAME_None);
	//~ End 효과음

	//~ Begin 음악
	//음악 상황을 알린다. 같은 상황이면 아무것도 하지 않는다.
	//표에서 정확한 태그 → 부모 태그 순서로 칸을 찾고, 끝까지 없으면 지금 곡을 그대로 둔다.
	//찾은 칸의 곡이 지금 곡과 같으면 곡을 끊지 않고 MetaSound 입력(Parameters)만 넘기고, 다르면 크로스페이드한다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Sound")
	void SetMusicState(FGameplayTag MusicTag);

	//마지막으로 알린 음악 상황
	UFUNCTION(BlueprintPure, Category = "Warrior|Sound")
	FGameplayTag GetMusicState() const { return CurrentMusicTag; }

	//지금 곡을 페이드 아웃하고 상황을 비운다
	UFUNCTION(BlueprintCallable, Category = "Warrior|Sound")
	void StopMusic(float FadeOutSeconds = 1.f);
	//~ End 음악

	//RootWidget 안의 모든 버튼(안에 들어 있는 위젯의 버튼까지)에 Sound.UI.Button.Click(누름)·Hover(올림) 소리를 넣는다.
	//이미 소리가 지정된 버튼은 건드리지 않는다. 위젯을 만든 뒤 한 번 부른다(나중에 동적으로 추가되는 버튼은 그때 다시 부른다)
	UFUNCTION(BlueprintCallable, Category = "Warrior|Sound")
	void ApplyButtonSounds(UUserWidget* RootWidget);

	//지금 쓰는 사운드 표 (없을 수 있음)
	const UWarriorSoundSet* GetSoundSet() const { return SoundSet; }

	//효과음 칸을 찾는다. 서브시스템이 없는 곳(애니메이션 에디터 미리 보기)에서는 설정의 표를 직접 읽는다
	static const FWarriorSoundEntry* FindSoundEntry(const UObject* WorldContextObject, const FGameplayTag& InSoundTag);

private:
	//한 번 재생 함수(PlaySound2D, PlaySoundAtLocation)로 낼 수 있는 칸인지 본다.
	//반복 소리는 한 번 재생으로 틀면 멈출 방법이 없으므로 막는다(엔진 PlaySound 노티파이와 같은 규칙)
	static bool CanPlayOneShot(const UObject* WorldContextObject, const FGameplayTag& InSoundTag, const FWarriorSoundEntry* InEntry);

	//표에 칸이 없는 태그를 처음 한 번만 로그로 남긴다
	void ReportMissingTag(const FGameplayTag& InTag);

	//페이드 아웃이 끝난 이전 곡 컴포넌트를 정리한다
	void PruneFadingMusic();

	UPROPERTY(Transient)
	TObjectPtr<UWarriorSoundSet> SoundSet;

	//지금 재생 중인 곡. 레벨을 넘어 유지되도록 월드가 아닌 오디오 장치에 붙어 있으므로 여기서 붙잡는다
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	//페이드 아웃 중인 이전 곡. 페이드가 끝나기 전에 정리되지 않게 붙잡아 둔다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> FadingMusicComponents;

	//마지막으로 알린 음악 상황
	FGameplayTag CurrentMusicTag;

	//지금 곡이 바뀔 때 쓸 페이드 아웃 시간(재생 중인 칸의 값)
	float PlayingFadeOutSeconds = 1.f;

	TSet<FGameplayTag> ReportedMissingTags;

	//반복 소리라서 한 번 재생을 막은 태그(경고를 한 번만 남기기 위함)
	TSet<FGameplayTag> ReportedLoopingTags;
};
