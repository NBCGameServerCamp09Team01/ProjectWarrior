// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_WarriorSound.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "WarriorSoundSubsystem.h"

FString UAnimNotify_WarriorSound::GetNotifyName_Implementation() const
{
	return SoundTag.IsValid() ? SoundTag.ToString() : Super::GetNotifyName_Implementation();
}

void UAnimNotify_WarriorSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	//엔진의 PlaySound 노티파이처럼 부모(블루프린트 이벤트)는 부르지 않는다.
	if (!MeshComp || !SoundTag.IsValid())
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	//애니메이션 에디터 미리 보기: 거리 감쇠 없이 화면 소리로 들려준다.
	if (World->WorldType == EWorldType::EditorPreview)
	{
		if (MeshComp->IsPlaying())
		{
			UWarriorSoundSubsystem::PlaySound2D(World, SoundTag);
		}
		return;
	}
#endif

	if (bFollowMesh)
	{
		UWarriorSoundSubsystem::SpawnSoundAttached(SoundTag, MeshComp, AttachName);
	}
	else
	{
		UWarriorSoundSubsystem::PlaySoundAtLocation(MeshComp, SoundTag, MeshComp->GetComponentLocation());
	}
}
