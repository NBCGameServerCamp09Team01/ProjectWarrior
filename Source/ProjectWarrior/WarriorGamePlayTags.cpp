// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorGamePlayTags.h"

namespace WarriorGameplayTags
{
	//InputTags
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look, "InputTag.Look");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_EquipWeapon, "InputTag.EquipWeapon");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_UnEquipWeapon, "InputTag.UnequipWeapon");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_LeftButton, "InputTag.LeftButton");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_LeftButton_Charge, "InputTag.LeftButton.Charge");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Dodge, "InputTag.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_DodgeRoll, "InputTag.DodgeRoll");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_LightAttack_Katana, "InputTag.LightAttack.Katana");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_HeavyAttack_Katana, "InputTag.HeavyAttack.Katana");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld, "InputTag.MustBeHeld");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld_Block, "InputTag.MustBeHeld.Block");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_MustBeHeld_Sprint, "InputTag.MustBeHeld.Sprint");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_Toggleable, "InputTag.Toggleable");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Toggleable_TargetLock, "InputTag.Toggleable.TargetLock");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_SwitchTarget, "InputTag.SwitchTarget");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_InventoryWheel, "InputTag.InventoryWheel");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_Interact, "InputTag.Interact");

	/** Player tags **/
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Equip_Weapon_Katana, "Player.Ability.Equip.Weapon.Katana");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Unequip_Weapon_Katana, "Player.Ability.Unequip.Weapon.Katana");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_HitStop, "Player.Ability.HitStop");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Block, "Player.Ability.Block");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Sprint, "Player.Ability.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Dodge, "Player.Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_DodgeRoll, "Player.Ability.DodgeRoll");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_TargetLock, "Player.Ability.TargetLock");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Finisher, "Player.Ability.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Counter, "Player.Ability.Counter");

	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Katana_Light_1, "Player.Ability.Attack.Katana.Light.1");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Katana_Light_2, "Player.Ability.Attack.Katana.Light.2");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Katana_Light_3, "Player.Ability.Attack.Katana.Light.3");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Katana_Light_4, "Player.Ability.Attack.Katana.Light.4");

	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Heavy_Katana, "Player.Ability.Attack.Heavy.Katana");

	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Katana, "Player.Weapon.Katana");

	UE_DEFINE_GAMEPLAY_TAG(Player_Event_HitStop, "Player.Event.HitStop");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Successful_Block, "Player.Event.Successful.Block");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Successful_Dodge, "Player.Event.Successful.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Finisher, "Player.Event.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Counter, "Player.Event.Counter");

	UE_DEFINE_GAMEPLAY_TAG(Player_Event_SwitchTarget_Left, "Player.Event.SwitchTarget.Left");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_SwitchTarget_Right, "Player.Event.SwitchTarget.Right");

	UE_DEFINE_GAMEPLAY_TAG(Player_ComboCheck, "Player.ComboCheck");

	UE_DEFINE_GAMEPLAY_TAG(Player_Event_ComboCheck_Begin, "Player.Event.ComboCheck.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_ComboCheck_End, "Player.Event.ComboCheck.End");

	UE_DEFINE_GAMEPLAY_TAG(Player_SetByCaller_AttackType_Light, "Player.SetByCaller.AttackType.Light");
	UE_DEFINE_GAMEPLAY_TAG(Player_SetByCaller_AttackType_Heavy, "Player.SetByCaller.AttackType.Heavy");

	UE_DEFINE_GAMEPLAY_TAG(Player_Status_Blocking, "Player.Status.Blocking");
	UE_DEFINE_GAMEPLAY_TAG(Player_Status_TargetLock, "Player.Status.TargetLock");
	UE_DEFINE_GAMEPLAY_TAG(Player_Status_OverrideLockOnRotation_Character, "Player.Status.OverrideLockOnRotation.Character");
	UE_DEFINE_GAMEPLAY_TAG(Player_Status_OverrideLockOnRotation_Controller, "Player.Status.OverrideLockOnRotation.Controller");
	UE_DEFINE_GAMEPLAY_TAG(Player_Status_RegenStamina, "Player.Status.RegenStamina");

	UE_DEFINE_GAMEPLAY_TAG(Player_Cooldown_Block, "Player.Cooldown.Block");

	/** AI tags **/
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Melee, "AI.Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Range, "AI.Ability.Range");
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Finisher, "AI.Ability.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Block, "AI.Ability.Block");

	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Equip_Weapon_Katana, "AI.Ability.Equip.Weapon.Katana");
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Unequip_Weapon_Katana, "AI.Ability.Unequip.Weapon.Katana");
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Equip_Weapon_Bow, "AI.Ability.Equip.Weapon.Bow");
	UE_DEFINE_GAMEPLAY_TAG(AI_Ability_Unequip_Weapon_Bow, "AI.Ability.Unequip.Weapon.Bow");

	UE_DEFINE_GAMEPLAY_TAG(AI_Weapon_Katana, "AI.Weapon.Katana");
	UE_DEFINE_GAMEPLAY_TAG(AI_Weapon_Bow, "AI.Weapon.Bow");

	UE_DEFINE_GAMEPLAY_TAG(AI_Event_Finisher, "AI.Event.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_Projectile_Spawn, "AI.Event.Projectile.Spawn");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_Projectile_Fire, "AI.Event.Projectile.Fire");

	UE_DEFINE_GAMEPLAY_TAG(AI_Status_Strafing, "AI.Status.Strafing");
	UE_DEFINE_GAMEPLAY_TAG(AI_Status_UnderAttack, "AI.Status.UnderAttack");
	UE_DEFINE_GAMEPLAY_TAG(AI_Status_Aiming, "AI.Status.Aiming");

	/** Shared tags **/
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_HitReact, "Shared.Ability.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_Death, "Shared.Ability.Death");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_Stagger, "Shared.Ability.Stagger");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_RotateToTarget, "Shared.Ability.RotateToTarget");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_MeleeHit, "Shared.Event.MeleeHit");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_HitReact, "Shared.Event.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_HitReact_KnockBack, "Shared.Event.HitReact.KnockBack");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_Equip_Weapon, "Shared.Event.Equip.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_Unequip_Weapon, "Shared.Event.Unequip.Weapon");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_RotateToTarget, "Shared.Event.RotateToTarget");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_Stagger, "Shared.Event.Stagger");

	UE_DEFINE_GAMEPLAY_TAG(Shared_SetByCaller_BaseDamage, "Shared.SetByCaller.BaseDamage");
	UE_DEFINE_GAMEPLAY_TAG(Shared_SetByCaller_Heal, "Shared.SetByCaller.Heal");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Death_Normal, "Shared.Status.Death.Normal");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Death_Finisher, "Shared.Status.Death.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Death_Knockback, "Shared.Status.Death.Knockback");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Front, "Shared.Status.HitReact.Front");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Left, "Shared.Status.HitReact.Left");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Right, "Shared.Status.HitReact.Right");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Back, "Shared.Status.HitReact.Back");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Dodge, "Shared.Status.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Sprint, "Shared.Status.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Finisher, "Shared.Status.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_WeaponEquipped, "Shared.Status.WeaponEquipped");
}