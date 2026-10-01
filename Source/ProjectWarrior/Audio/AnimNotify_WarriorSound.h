// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_WarriorSound.generated.h"

/**
 * 몽타주·애니메이션에서 태그로 소리를 내는 노티파이 (몽타주 타임라인에서 "Warrior Sound").
 * 몽타주에는 태그만 들어가고, 어떤 소리를 낼지는 사운드 표(UWarriorSoundSet)가 정한다.
 * 그래서 소리를 바꿀 때 몽타주를 다시 저장할 필요가 없다.
 * 애니메이션 에디터 미리 보기에서도 설정의 표를 읽어 소리를 들려준다.
 *
 * 주의: 플레이어와 몬스터가 같은 몽타주를 쓰는 곳(예: GhostSamurai_APose_Attack01_1_ALL_Root_Montage)에
 * 넣으면 양쪽에서 같은 소리가 난다. 그런 곳은 코드(전투 컴포넌트)에서 태그를 나눠 부른다.
 */
UCLASS(meta = (DisplayName = "Warrior Sound"))
class PROJECTWARRIOR_API UAnimNotify_WarriorSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	//~ Begin UAnimNotify Interface.
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	//~ End UAnimNotify Interface

	//낼 소리의 태그 (Sound.*)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify", meta = (Categories = "Sound"))
	FGameplayTag SoundTag;

	//켜면 메시에 붙어 따라다닌다. 끄면 그 순간의 메시 위치에서 한 번 난다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify")
	bool bFollowMesh = false;

	//따라다닐 때 붙을 소켓·본 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimNotify", meta = (EditCondition = "bFollowMesh"))
	FName AttachName;
};
