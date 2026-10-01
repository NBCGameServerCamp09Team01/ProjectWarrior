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
 *   한쪽(플레이어 또는 몬스터)만 쓰는 몽타주의 동작 소리(Warrior Sound 노티파이).
 * - GameplayCue: 전투 판정·GameplayEffect 결과(휘두르기, 패링, 회피 성공, 명중, 아이템·버프 효과, 보스 공격).
 *   코드는 GameplayCue.* 태그로 큐를 실행하고, 큐 에셋(GCN Warrior Burst)의 Sound Tag가 아래 "전투 소리 칸"을 가리킨다.
 *   전투 소리 칸은 코드에서 직접 부르지 않는다.
 */
namespace WarriorSoundTags
{
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

	// 플레이어 동작 (Warrior Sound 노티파이, 플레이어 전용 몽타주)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Roll);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Dodge);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Death);

	// 몬스터 동작·액터 (활 시위는 궁수 전용 몽타주의 노티파이, 화살 비행은 SpawnSoundAttached)
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Bow_Release);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Arrow_Fly);

	// 상점
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_Open);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_Close);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_Purchase);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Shop_NotEnoughGold);

	// 전투 소리 칸: GameplayCue 큐 에셋(GCN Warrior Burst)의 Sound Tag로만 지정한다. 코드에서 직접 부르지 않는다
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Attack_Swing);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Parry);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Player_Dodge_Perfect);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Enemy_Melee_Swing);
	PROJECTWARRIOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Item_Use);
}
