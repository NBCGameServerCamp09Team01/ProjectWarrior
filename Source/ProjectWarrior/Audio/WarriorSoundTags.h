// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 사운드 전용 네이티브 태그.
 * 공용 태그 파일(WarriorGamePlayTags)과 분리해 다른 작업과 충돌하지 않게 한다.
 *
 * - Music.* : 음악 상황. UWarriorSoundSubsystem::SetMusicState로 알린다.
 * - Sound.* : 효과음. UWarriorSoundSubsystem::PlaySound2D 등으로 재생한다.
 *
 * 어떤 소리를 쓸지는 사운드 표(UWarriorSoundSet)에서 태그마다 정한다. 코드는 태그만 부른다.
 * 표에 정확한 태그가 없으면 부모 태그로 올라가며 찾는다.
 * 그래서 하위 태그(예: Sound.Player.Attack.Swing.Fire, Music.Stage.Boss.<보스>)를 새로 만들어 불러도
 * 표에 칸을 넣기 전까지는 부모 태그의 소리가 그대로 난다.
 *
 * 누가 이 태그를 부르나 (사운드 시스템 명세 SoundSystem_20261001 1장)
 * - 우리 틀(UWarriorSoundSubsystem): 음악, UI·화면 흐름·상점·성장, ASC가 없는 액터(날아가는 화살),
 *   휘두르기처럼 한쪽(플레이어 또는 몬스터)만 쓰는 몽타주·어빌리티의 동작 소리.
 * - GameplayCue: 전투 판정·GameplayEffect 결과(패링, 가드, 회피 성공, 명중, 아이템·버프 효과, 보스 공격).
 *   코드는 GameplayCue.* 태그로 큐를 실행하고, 큐 에셋(GCN Warrior Burst)의 Sound Tag가 아래 "전투 결과 소리 칸"을 가리킨다.
 *   [프로토타입] 가드·패링은 지금 막기 어빌리티(GA_Player_Block)에서 직접 부른다. 이펙트를 붙일 때 GameplayCue로 옮긴다(기술부채 TD-4).
 */
namespace WarriorSoundTags
{
	// 음악 부모: 표에 칸을 두면 그 아래 상황 전체의 기본값이 된다(하위 칸이 있으면 하위가 우선).
	// 예) Music.Stage 칸의 소리를 비워 두면 스테이지는 무음, Music.Stage.Boss 칸만 보스 곡
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Front);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Stage);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Result);

	// 음악: 프론트(타이틀·메인메뉴)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Front_Title);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Front_MainMenu);

	// 음악: 스테이지
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Stage_Prepare);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Stage_Normal);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Stage_Boss);

	// 음악: 결과
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Result_Cleared);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Result_Failed);

	// UI
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Button_Click);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Button_Hover);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Front_Start);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Stage_WaveCleared);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Growth_Invest);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Growth_Rejected);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_UI_Growth_Unlock);

	// 플레이어 휘두르기 (콤보 어빌리티 GA_Katana_LightAttack_01~04에서 부름. 콤보 칸이 없으면 Swing 칸)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Attack_Swing);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Attack_Swing_Combo1);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Attack_Swing_Combo2);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Attack_Swing_Combo3);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Attack_Swing_Combo4);

	// 플레이어 동작 (Warrior Sound 노티파이, 플레이어 전용 몽타주)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Roll);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Dodge);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Death);

	// 몬스터 동작·액터 (근접 휘두르기·활 시위는 몬스터 전용 몽타주의 노티파이, 화살 비행은 SpawnSoundAttached)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Melee_Swing);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Bow_Release);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Arrow_Fly);

	// 상점
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_Open);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_Close);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_Purchase);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_NotEnoughGold);

	// 전투 결과 소리 칸: GameplayCue 큐 에셋(GCN Warrior Burst)의 Sound Tag로 지정한다.
	// [프로토타입] Guard·Parry는 지금 GA_Player_Block에서, Enemy.Hit는 콤보 어빌리티 명중 지점에서 직접 부른다(TD-4)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Guard);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Parry);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Hit);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Hit_Heavy);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Dodge_Perfect);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Item_Use);
}
