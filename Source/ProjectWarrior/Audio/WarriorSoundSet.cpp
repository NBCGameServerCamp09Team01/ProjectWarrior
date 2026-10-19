// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSoundSet.h"

namespace
{
	//정확한 태그부터 부모 태그로 올라가며 칸을 찾는다
	template <typename EntryType>
	const EntryType* FindWithParents(const TMap<FGameplayTag, EntryType>& InMap, const FGameplayTag& InTag, FGameplayTag* OutMatchedTag)
	{
		for (FGameplayTag Tag = InTag; Tag.IsValid(); Tag = Tag.RequestDirectParent())
		{
			if (const EntryType* Found = InMap.Find(Tag))
			{
				if (OutMatchedTag)
				{
					*OutMatchedTag = Tag;
				}
				return Found;
			}
		}

		return nullptr;
	}
}

const FWarriorSoundEntry* UWarriorSoundSet::FindSound(const FGameplayTag& InTag, FGameplayTag* OutMatchedTag) const
{
	return FindWithParents(Sounds, InTag, OutMatchedTag);
}

const FWarriorMusicEntry* UWarriorSoundSet::FindMusic(const FGameplayTag& InTag, FGameplayTag* OutMatchedTag) const
{
	return FindWithParents(Music, InTag, OutMatchedTag);
}
