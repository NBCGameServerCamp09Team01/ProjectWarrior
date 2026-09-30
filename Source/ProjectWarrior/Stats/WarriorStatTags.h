#pragma once

#include "NativeGameplayTags.h"

/**
 * 통계 전용 네이티브 태그 (FWarriorStatBlock::Extra 키).
 * 공용 태그 파일(WarriorGamePlayTags)과 분리해 다른 작업과 충돌하지 않게 한다.
 * 새 통계는 S1 계획서 8장 표에 먼저 적고 여기에 추가한다.
 */
namespace WarriorStatTags
{
	// 공격
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Combat_Damage_Overkill);		// Sum, 세부 키: 적 종류
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Combat_Balance_Dealt);		// Sum, 세부 키: 적 종류
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Combat_Stagger_Caused);		// Count, 세부 키: 적 종류

	// 피격
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_Health_MinRatio);	// Min
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_Wave_NoHit);			// Count

	// 시간·진행
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Time_Rest);					// Sum
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Time_Shop);					// Sum
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Progress_Rest_Skipped);		// Count
}
