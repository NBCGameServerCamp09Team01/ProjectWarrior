// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WarriorSkillEffectSettings.generated.h"

/**
 * 프로젝트 설정 > Game > Warrior Skill Effects. 해금한 스킬의 효과 수치(S4). 값은 Config/DefaultGame.ini에 저장된다.
 * 해금 여부는 캐릭터가 스테이지에 들어올 때 능력 시스템에 붙이는 태그(Account.Skill.*, AWarriorPlayerCharacter::ApplyAccountSkillTags)로 본다.
 * 효과를 거는 곳(UPlayerGameplayAbility): 반격 강화 = 데미지·균형 데미지 스펙을 만들 때(ApplyCounterPlus), 회피 숙련·콤보 4타 = CanActivateAbility.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Warrior Skill Effects"))
class PROJECTWARRIOR_API UWarriorSkillEffectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//반격 강화(Account.Skill.CounterPlus): 반격 능력(Player.Ability.Counter)의 기본 데미지 배율
	UPROPERTY(Config, EditAnywhere, Category = "Skill", meta = (ClampMin = "0"))
	float CounterPlusDamageMultiplier = 1.5f;

	//회피 숙련(Account.Skill.DodgeMastery): 켜져 있으면 회피(Player.Ability.Dodge, 더블 탭 스텝)는 스킬을 해금해야 나간다.
	//시연용 효과(10/12). 끄면 예전처럼 늘 나간다. 구르기(DodgeRoll)는 다른 능력이라 막지 않는다
	UPROPERTY(Config, EditAnywhere, Category = "Skill")
	bool bDodgeRequiresDodgeMastery = true;

	//콤보 4타(Account.Skill.Combo4): 켜져 있으면 약공격 4타(Player.Ability.Attack.Katana.Light.4)는 스킬을 해금해야 나간다.
	//해금 전에는 3타에서 콤보가 끝난다. 끄면 예전처럼 4타까지 나간다
	UPROPERTY(Config, EditAnywhere, Category = "Skill")
	bool bCombo4RequiresUnlock = true;
};
