// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorAccountTags.h"

namespace WarriorAccountTags
{
	// 투자 가능한 스탯
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Stat_MaxHealth, "Account.Stat.MaxHealth", "최대 체력 투자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Stat_MaxStamina, "Account.Stat.MaxStamina", "최대 스태미나 투자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Stat_AttackPower, "Account.Stat.AttackPower", "공격력 투자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Stat_DefensePower, "Account.Stat.DefensePower", "방어력 투자");

	// 해금 가능한 스킬
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Skill_Combo4, "Account.Skill.Combo4", "콤보 4타 해금");

	// 조건 해금 스킬 (SK, 목록은 DA_SkillTree)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Skill_HeavyAttack, "Account.Skill.HeavyAttack", "강공격 해금");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Skill_HeavyAttackPlus, "Account.Skill.HeavyAttackPlus", "강공격 강화 해금");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Skill_FinisherMastery, "Account.Skill.FinisherMastery", "처형 숙련 해금");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Skill_DodgeMastery, "Account.Skill.DodgeMastery", "회피 숙련 해금");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Account_Skill_CounterPlus, "Account.Skill.CounterPlus", "반격 강화 해금");
}
