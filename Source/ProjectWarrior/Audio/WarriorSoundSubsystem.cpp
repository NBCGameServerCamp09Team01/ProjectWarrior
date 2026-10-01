// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSoundSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "ProjectWarrior/ProjectWarrior.h"
#include "WarriorSoundSet.h"
#include "WarriorSoundSettings.h"
#include "WarriorSoundTags.h"

UWarriorSoundSubsystem* UWarriorSoundSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UWarriorSoundSubsystem>() : nullptr;
}

void UWarriorSoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SoundSet = UWarriorSoundSettings::LoadSoundSet();
	if (SoundSet)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] Sound set %s"), *GetNameSafe(SoundSet));
	}
	else
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Sound] Sound set is not set (Project Settings > Game > Warrior Sound). No sound is played."));
	}
}

void UWarriorSoundSubsystem::Deinitialize()
{
	//레벨을 넘어 유지되는 곡은 월드가 정리해 주지 않으므로 여기서 멈춘다(PIE 종료·게임 종료).
	auto StopAndDestroy = [](UAudioComponent* InComponent)
	{
		if (IsValid(InComponent))
		{
			InComponent->Stop();
			InComponent->DestroyComponent();
		}
	};

	StopAndDestroy(MusicComponent);
	for (UAudioComponent* FadingComponent : FadingMusicComponents)
	{
		StopAndDestroy(FadingComponent);
	}

	MusicComponent = nullptr;
	FadingMusicComponents.Reset();
	CurrentMusicTag = FGameplayTag();
	SoundSet = nullptr;

	Super::Deinitialize();
}

const FWarriorSoundEntry* UWarriorSoundSubsystem::FindSoundEntry(const UObject* WorldContextObject, const FGameplayTag& InSoundTag)
{
	if (!InSoundTag.IsValid())
	{
		return nullptr;
	}

	if (UWarriorSoundSubsystem* Subsystem = Get(WorldContextObject))
	{
		const FWarriorSoundEntry* Entry = Subsystem->SoundSet ? Subsystem->SoundSet->FindSound(InSoundTag) : nullptr;
		if (!Entry)
		{
			Subsystem->ReportMissingTag(InSoundTag);
		}
		return Entry;
	}

	//GameInstance가 없는 곳(애니메이션 에디터 미리 보기 등)에서는 설정의 표를 직접 읽는다.
	const UWarriorSoundSet* Set = UWarriorSoundSettings::LoadSoundSet();
	return Set ? Set->FindSound(InSoundTag) : nullptr;
}

bool UWarriorSoundSubsystem::CanPlayOneShot(const UObject* WorldContextObject, const FGameplayTag& InSoundTag, const FWarriorSoundEntry* InEntry)
{
	if (!InEntry || !InEntry->Sound)
	{
		return false;
	}

	if (InEntry->Sound->IsOneShot())
	{
		return true;
	}

	UWarriorSoundSubsystem* Subsystem = Get(WorldContextObject);
	if (!Subsystem || !Subsystem->ReportedLoopingTags.Contains(InSoundTag))
	{
		if (Subsystem)
		{
			Subsystem->ReportedLoopingTags.Add(InSoundTag);
		}
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Sound] %s is a looping sound (%s). One-shot play is skipped. Use SpawnSoundAttached or the music table."),
			*InSoundTag.ToString(), *GetNameSafe(InEntry->Sound));
	}
	return false;
}

void UWarriorSoundSubsystem::PlaySound2D(const UObject* WorldContextObject, FGameplayTag SoundTag)
{
	const FWarriorSoundEntry* Entry = FindSoundEntry(WorldContextObject, SoundTag);
	if (CanPlayOneShot(WorldContextObject, SoundTag, Entry))
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, Entry->Sound, Entry->VolumeMultiplier, Entry->PitchMultiplier);
	}
}

void UWarriorSoundSubsystem::PlaySoundAtLocation(const UObject* WorldContextObject, FGameplayTag SoundTag, FVector Location)
{
	const FWarriorSoundEntry* Entry = FindSoundEntry(WorldContextObject, SoundTag);
	if (CanPlayOneShot(WorldContextObject, SoundTag, Entry))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Entry->Sound, Location, FRotator::ZeroRotator,
			Entry->VolumeMultiplier, Entry->PitchMultiplier, 0.f, Entry->Attenuation);
	}
}

UAudioComponent* UWarriorSoundSubsystem::SpawnSoundAttached(FGameplayTag SoundTag, USceneComponent* AttachToComponent, FName AttachPointName)
{
	if (!AttachToComponent)
	{
		return nullptr;
	}

	const FWarriorSoundEntry* Entry = FindSoundEntry(AttachToComponent, SoundTag);
	if (!Entry || !Entry->Sound)
	{
		return nullptr;
	}

	return UGameplayStatics::SpawnSoundAttached(Entry->Sound, AttachToComponent, AttachPointName, FVector(ForceInit), FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true, Entry->VolumeMultiplier, Entry->PitchMultiplier, 0.f, Entry->Attenuation);
}

void UWarriorSoundSubsystem::SetMusicState(FGameplayTag MusicTag)
{
	if (MusicTag == CurrentMusicTag)
	{
		return;
	}

	const FGameplayTag PreviousTag = CurrentMusicTag;
	CurrentMusicTag = MusicTag;
	PruneFadingMusic();

	FGameplayTag MatchedTag;
	const FWarriorMusicEntry* Entry = SoundSet ? SoundSet->FindMusic(MusicTag, &MatchedTag) : nullptr;
	if (!Entry)
	{
		//표에 칸이 없으면 지금 곡을 그대로 둔다. 표를 일부만 채워도 동작하게 하기 위함.
		ReportMissingTag(MusicTag);
		UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] Music %s -> %s. No entry, keep current music."),
			*PreviousTag.ToString(), *MusicTag.ToString());
		return;
	}

	//같은 곡이면 끊지 않고 MetaSound 입력과 볼륨만 바꾼다(곡 안에서 구간 이동·반복).
	const bool bIsPlaying = MusicComponent && MusicComponent->IsPlaying();
	if (Entry->Sound && bIsPlaying && MusicComponent->Sound == Entry->Sound)
	{
		if (Entry->Parameters.Num() > 0)
		{
			TArray<FAudioParameter> Parameters = Entry->Parameters;
			MusicComponent->SetParameters(MoveTemp(Parameters));
		}
		MusicComponent->SetVolumeMultiplier(Entry->Volume);
		PlayingFadeOutSeconds = Entry->FadeOutSeconds;

		UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] Music %s -> %s (entry %s). Same sound %s, %d parameters."),
			*PreviousTag.ToString(), *MusicTag.ToString(), *MatchedTag.ToString(), *GetNameSafe(Entry->Sound), Entry->Parameters.Num());
		return;
	}

	//다른 곡(또는 무음)이면 지금 곡을 페이드 아웃한다. 페이드가 끝날 때까지 붙잡아 둔다.
	if (MusicComponent)
	{
		MusicComponent->FadeOut(PlayingFadeOutSeconds, 0.f);
		FadingMusicComponents.Add(MusicComponent);
		MusicComponent = nullptr;
	}

	if (!Entry->Sound)
	{
		UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] Music %s -> %s (entry %s). Silence."),
			*PreviousTag.ToString(), *MusicTag.ToString(), *MatchedTag.ToString());
		return;
	}

	//레벨을 넘어 유지되는 곡으로 만든다(타이틀 → 메인메뉴 → 스테이지 사이에 끊기지 않음).
	UAudioComponent* NewComponent = UGameplayStatics::CreateSound2D(GetGameInstance(), Entry->Sound, Entry->Volume, 1.f, Entry->StartTime,
		nullptr, true, false);
	if (!NewComponent)
	{
		UE_LOG(LogProjectWarrior, Warning, TEXT("[Sound] Music %s could not be created (%s)."), *MusicTag.ToString(), *GetNameSafe(Entry->Sound));
		return;
	}

	if (Entry->Parameters.Num() > 0)
	{
		TArray<FAudioParameter> Parameters = Entry->Parameters;
		NewComponent->SetParameters(MoveTemp(Parameters));
	}
	NewComponent->FadeIn(Entry->FadeInSeconds, 1.f, Entry->StartTime);

	MusicComponent = NewComponent;
	PlayingFadeOutSeconds = Entry->FadeOutSeconds;

	UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] Music %s -> %s (entry %s). Play %s, fade in %.1f s."),
		*PreviousTag.ToString(), *MusicTag.ToString(), *MatchedTag.ToString(), *GetNameSafe(Entry->Sound), Entry->FadeInSeconds);
}

void UWarriorSoundSubsystem::StopMusic(float FadeOutSeconds)
{
	PruneFadingMusic();

	if (MusicComponent)
	{
		MusicComponent->FadeOut(FMath::Max(0.f, FadeOutSeconds), 0.f);
		FadingMusicComponents.Add(MusicComponent);
		MusicComponent = nullptr;
	}

	UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] Music %s stopped (fade out %.1f s)."), *CurrentMusicTag.ToString(), FadeOutSeconds);
	CurrentMusicTag = FGameplayTag();
}

void UWarriorSoundSubsystem::ApplyButtonSounds(UUserWidget* RootWidget)
{
	if (!RootWidget || !SoundSet)
	{
		return;
	}

	const FWarriorSoundEntry* ClickEntry = SoundSet->FindSound(WarriorSoundTags::Sound_UI_Button_Click);
	const FWarriorSoundEntry* HoverEntry = SoundSet->FindSound(WarriorSoundTags::Sound_UI_Button_Hover);
	USoundBase* ClickSound = ClickEntry ? ClickEntry->Sound.Get() : nullptr;
	USoundBase* HoverSound = HoverEntry ? HoverEntry->Sound.Get() : nullptr;
	if (!ClickSound && !HoverSound)
	{
		return;
	}

	//버튼 소리는 Slate가 재생하므로 표의 볼륨·피치는 쓰지 않는다(소리 에셋 쪽에서 조절).
	int32 ChangedButtonCount = 0;
	TArray<UUserWidget*> PendingWidgets = { RootWidget };
	TSet<UUserWidget*> VisitedWidgets;

	while (PendingWidgets.Num() > 0)
	{
		UUserWidget* CurrentWidget = PendingWidgets.Pop(EAllowShrinking::No);
		if (!CurrentWidget || !CurrentWidget->WidgetTree || VisitedWidgets.Contains(CurrentWidget))
		{
			continue;
		}
		VisitedWidgets.Add(CurrentWidget);

		CurrentWidget->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (UButton* Button = Cast<UButton>(Widget))
			{
				//디자이너가 따로 넣은 소리는 그대로 둔다.
				FButtonStyle Style = Button->GetStyle();
				bool bChanged = false;

				if (ClickSound && !Style.PressedSlateSound.GetResourceObject())
				{
					Style.PressedSlateSound.SetResourceObject(ClickSound);
					bChanged = true;
				}

				if (HoverSound && !Style.HoveredSlateSound.GetResourceObject())
				{
					Style.HoveredSlateSound.SetResourceObject(HoverSound);
					bChanged = true;
				}

				if (bChanged)
				{
					Button->SetStyle(Style);
					++ChangedButtonCount;
				}
			}
			else if (UUserWidget* ChildWidget = Cast<UUserWidget>(Widget))
			{
				//안에 들어 있는 위젯(예: 목록 항목)의 버튼도 찾는다.
				PendingWidgets.Add(ChildWidget);
			}
		});
	}

	UE_LOG(LogProjectWarrior, Verbose, TEXT("[Sound] Button sounds applied to %d buttons in %s."), ChangedButtonCount, *GetNameSafe(RootWidget));
}

void UWarriorSoundSubsystem::ReportMissingTag(const FGameplayTag& InTag)
{
	if (!InTag.IsValid() || ReportedMissingTags.Contains(InTag))
	{
		return;
	}

	ReportedMissingTags.Add(InTag);
	UE_LOG(LogProjectWarrior, Log, TEXT("[Sound] No entry for %s in %s. Nothing is played."), *InTag.ToString(), *GetNameSafe(SoundSet));
}

void UWarriorSoundSubsystem::PruneFadingMusic()
{
	for (int32 Index = FadingMusicComponents.Num() - 1; Index >= 0; --Index)
	{
		UAudioComponent* FadingComponent = FadingMusicComponents[Index];
		if (!IsValid(FadingComponent) || !FadingComponent->IsPlaying())
		{
			if (IsValid(FadingComponent))
			{
				FadingComponent->DestroyComponent();
			}
			FadingMusicComponents.RemoveAtSwap(Index);
		}
	}
}
