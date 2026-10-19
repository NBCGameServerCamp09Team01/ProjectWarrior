// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 계정 성장 전용 네이티브 태그.
 * 공용 태그 파일(WarriorGamePlayTags)과 분리해 다른 작업과 충돌하지 않게 한다.
 *
 * - Account.Stat.*  : 스탯 포인트를 투자할 수 있는 스탯. GAS 어트리뷰트와 1:1로 연결된다
 *                     (연결 표는 UWarriorAccountSubsystem의 스탯 정의).
 * - Account.Skill.* : 스탯 포인트로 해금하는 스킬.
 * 새 스탯·스킬은 여기에 태그를 추가하고 서브시스템의 정의 표에 한 줄을 더한다.
 */
namespace WarriorAccountTags
{
	// 투자 가능한 스탯
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Stat_MaxHealth);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Stat_MaxStamina);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Stat_AttackPower);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Stat_DefensePower);

	// 해금 가능한 스킬
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Skill_Combo4);

	// 조건 해금 스킬 (SK, 목록은 DA_SkillTree)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Skill_HeavyAttack);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Skill_HeavyAttackPlus);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Skill_FinisherMastery);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Skill_DodgeMastery);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Account_Skill_CounterPlus);
}
