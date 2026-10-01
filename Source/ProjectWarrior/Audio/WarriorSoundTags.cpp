// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorSoundTags.h"

namespace WarriorSoundTags
{
	// 음악 부모
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Front, "Music.Front", "프론트(타이틀·메인메뉴·성장) 기본 곡");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Stage, "Music.Stage", "스테이지 기본 곡(소리를 비우면 무음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Result, "Music.Result", "결과 화면 기본 곡");

	// 음악: 프론트(타이틀·메인메뉴)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Front_Title, "Music.Front.Title", "타이틀 화면");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Front_MainMenu, "Music.Front.MainMenu", "메인메뉴·성장 화면");

	// 음악: 스테이지
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Stage_Prepare, "Music.Stage.Prepare", "준비·쉬는 시간");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Stage_Normal, "Music.Stage.Normal", "일반 웨이브");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Stage_Boss, "Music.Stage.Boss", "보스 웨이브. 보스별 음악은 하위 태그(Music.Stage.Boss.<보스>)");

	// 음악: 결과
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Result_Cleared, "Music.Result.Cleared", "클리어 결과(한 번 재생하는 소리도 가능)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Music_Result_Failed, "Music.Result.Failed", "실패 결과. 결과 화면이 보일 때");

	// UI
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Button_Click, "Sound.UI.Button.Click", "버튼 클릭(ApplyButtonSounds가 일괄 적용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Button_Hover, "Sound.UI.Button.Hover", "버튼 마우스 올림(ApplyButtonSounds가 일괄 적용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Front_Start, "Sound.UI.Front.Start", "타이틀에서 메인메뉴로 넘어갈 때");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Stage_WaveCleared, "Sound.UI.Stage.WaveCleared", "일반 웨이브 클리어");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Growth_Invest, "Sound.UI.Growth.Invest", "스탯 투자 성공(확정음 겸용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Growth_Rejected, "Sound.UI.Growth.Rejected", "스탯 투자 실패(포인트 부족 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_UI_Growth_Unlock, "Sound.UI.Growth.Unlock", "스킬 해금");

	// 플레이어 휘두르기 (콤보 어빌리티에서 부름)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Attack_Swing, "Sound.Player.Attack.Swing", "플레이어 칼 휘두르기 기본(콤보 칸이 없을 때). 불 칼은 하위 태그");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Attack_Swing_Combo1, "Sound.Player.Attack.Swing.Combo1", "콤보 1타(GA_Katana_LightAttack_01)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Attack_Swing_Combo2, "Sound.Player.Attack.Swing.Combo2", "콤보 2타(GA_Katana_LightAttack_02)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Attack_Swing_Combo3, "Sound.Player.Attack.Swing.Combo3", "콤보 3타(GA_Katana_LightAttack_03)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Attack_Swing_Combo4, "Sound.Player.Attack.Swing.Combo4", "콤보 4타·마무리(GA_Katana_LightAttack_04)");

	// 플레이어 동작 (Warrior Sound 노티파이, 플레이어 전용 몽타주)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Roll, "Sound.Player.Roll", "구르기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Dodge, "Sound.Player.Dodge", "회피 동작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Death, "Sound.Player.Death", "플레이어 사망(AM_Player_Death)");

	// 몬스터 동작·액터
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Enemy_Melee_Swing, "Sound.Enemy.Melee.Swing", "근거리 몬스터 칼 휘두르기(몬스터 전용 공격 몽타주 노티파이)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Enemy_Bow_Release, "Sound.Enemy.Bow.Release", "활 시위(AM_BowShot 노티파이)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Enemy_Arrow_Fly, "Sound.Enemy.Arrow.Fly", "화살이 날아가는 동안(SpawnSoundAttached)");

	// 상점
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Shop_Open, "Sound.Shop.Open", "상점 열기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Shop_Close, "Sound.Shop.Close", "상점 닫기");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Shop_Purchase, "Sound.Shop.Purchase", "구매 성공(돈 쓰는 소리)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Shop_NotEnoughGold, "Sound.Shop.NotEnoughGold", "골드 부족");

	// 전투 결과 소리 칸: GameplayCue 큐 에셋(GCN Warrior Burst)의 Sound Tag로 지정한다. 가드·패링은 막기 어빌리티, 몬스터 피격은 콤보 어빌리티에서 지금 직접 부름(TD-4)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Guard, "Sound.Player.Guard", "막기 성공·가드 피격 쇳소리(GA_Player_Block)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Parry, "Sound.Player.Parry", "패링·칼 튕김 쇳소리(GA_Player_Block, 막기 직후 0.25초 안)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Enemy_Hit, "Sound.Enemy.Hit", "몬스터 피격(플레이어 콤보 1~3타 명중, GA_Katana_LightAttack_01~03)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Enemy_Hit_Heavy, "Sound.Enemy.Hit.Heavy", "몬스터 강한 피격(콤보 4타 명중, GA_Katana_LightAttack_04). 칸이 없으면 Sound.Enemy.Hit");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Player_Dodge_Perfect, "Sound.Player.Dodge.Perfect", "회피 성공·저스트 회피(GameplayCue)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Item_Use, "Sound.Item.Use", "아이템 사용 효과(아이템 UseEffect의 GameplayCue). 아이템별 소리는 하위 태그");
}
