#include <melee/rogue/rogue_state.h>
#include <melee/rogue/rogue_hooks.h>
#include <melee/ft/types.h>
#include <melee/pl/forward.h>
#include <melee/it/forward.h>
#include <assert.h>
#include <melee/rogue/rogue_effects.h>
#include <melee/ft/kinds/ftCommon/forward.h>
#include <math.h>
#include <stdio.h>

RogueRun g_rogue_run;
static u8 current_mode;
void Player_SetHPByIndex(s32 slot, s32 sub, s32 hp) {}
static Gm_PKind player_type = Gm_PKind_Human;
bool Rogue_IsActive(void) { return g_rogue_run.active; }
u8 gm_GetCurrentGameMode(void) { return current_mode; }
Gm_PKind Player_GetPlayerSlotType(s32 slot) { return player_type; }

static void check(const Fighter* fighter, bool modified)
{
    assert(Rogue_IsRunPlayer(fighter) == modified);
    assert(fabsf(Rogue_ModifyDamageDealt(fighter, 10) - (modified ? 12 : 10)) < .001f);
    assert(fabsf(Rogue_ModifyDamageReceived(fighter, 10) - (modified ? 8 : 10)) < .001f);
    assert(fabsf(Rogue_ModifyRunSpeed(fighter, 10) - (modified ? 12 : 10)) < .001f);
    assert(fabsf(Rogue_ModifyShieldHealth(fighter, 60) - (modified ? 78 : 60)) < .001f);
    assert(Rogue_ModifyDamageReceived(fighter, -2) == -2);
}

int main(void)
{
    Fighter fighter = { 0 };
    int mode;
    g_rogue_run.stats = (RogueStats) { 1.2f, .8f, 1.2f, 1.3f };
    /* Stale run data must never affect any other game mode. */
    g_rogue_run.active = true;
    for (mode = 0; mode < GM_COUNT; ++mode) {
        current_mode = mode;
        check(&fighter, mode == GM_ROGUE);
    }
    current_mode = GM_ROGUE;
    current_mode = GM_ADVENTURE;
    check(&fighter, false);
    current_mode = GM_CLASSIC;
    check(&fighter, false);
    current_mode = GM_ROGUE;
    fighter.player_id = 1;
    check(&fighter, false);
    fighter.player_id = 0;
    fighter.is_sub_fighter = true;
    check(&fighter, false);
    fighter.is_sub_fighter = false;
    player_type = Gm_PKind_Cpu;
    check(&fighter, false);
    player_type = Gm_PKind_Human;
    check(NULL, false);
    {
        Fighter enemy = { 0 };
        enemy.player_id = 1;
        enemy.dmg.x1830_percent = 101;
        g_rogue_run.stats.executioner_bonus = .25f;
        g_rogue_run.stats.aerial_damage_bonus = .18f;
        g_rogue_run.stats.smash_damage_bonus = .20f;
        g_rogue_run.stats.knockback_dealt_bonus = .12f;
        g_rogue_run.stats.knockback_resistance = .12f;
        g_rogue_run.stats.shield_stun_reduction = .15f;
        fighter.motion_id = ftCo_MS_AttackAirN;
        assert(fabsf(Rogue_ModifyAttackDamage(&fighter, &enemy, 10, false) - 16.3f) < .001f);
        assert(fabsf(Rogue_ModifyAttackDamage(&fighter, &enemy, 10, true) - 14.5f) < .001f);
        enemy.dmg.x1830_percent = 100;
        fighter.motion_id = ftCo_MS_AttackHi4;
        assert(fabsf(Rogue_ModifyAttackDamage(&fighter, &enemy, 10, false) - 14) < .001f);
        assert(fabsf(Rogue_ModifyKnockback(&fighter, &enemy, 100) - 112) < .001f);
        assert(fabsf(Rogue_ModifyKnockback(&enemy, &fighter, 100) - 88) < .001f);
        assert(fabsf(Rogue_ModifyShieldStun(&fighter, 20) - 17) < .001f);
        for (mode = 0; mode < GM_COUNT; ++mode) {
            current_mode = mode;
            if (mode == GM_ROGUE) continue;
            assert(Rogue_ModifyAttackDamage(&fighter, &enemy, 10, false) == 10);
            assert(Rogue_ModifyKnockback(&fighter, &enemy, 100) == 100);
            assert(Rogue_ModifyShieldStun(&fighter, 20) == 20);
        }
        current_mode = GM_ROGUE;
    }
    {
        Fighter enemy = { 0 };
        enemy.player_id = 1;
        g_rogue_run.stats.combo_bonus = .03f;
        g_rogue_run.stats.momentum_bonus = .03f;
        g_rogue_run.stats.static_effect = true;
        g_rogue_run.stats.second_shell = true;
        g_rogue_run.stats.parry_heal = 2;
        fighter.motion_id = ftCo_MS_Wait;
        RogueEffects_BeginEncounter();
        assert(!RogueEffects_OnHit(&fighter, &enemy, 0));
        assert(!RogueEffects_OnHit(&enemy, &fighter, 10));
        assert(g_rogue_run.effects.hit_count == 0);
        for (int hit = 1; hit <= 10; ++hit)
            assert(RogueEffects_OnHit(&fighter, &enemy, 10) == (hit % 5 == 0));
        assert(g_rogue_run.effects.combo_stacks == 5);
        assert(fabsf(Rogue_ModifyAttackDamage(&fighter, &enemy, 10, false) - 13.5f) < .001f);
        assert(fabsf(Rogue_ModifyRunSpeed(&fighter, 10) - 12.3f) < .001f);
        for (int frame = 0; frame < 119; ++frame) RogueEffects_OnFrame();
        assert(g_rogue_run.effects.combo_stacks == 5);
        RogueEffects_OnFrame();
        assert(g_rogue_run.effects.combo_stacks == 0);
        assert(g_rogue_run.effects.momentum_frames == 60);
        for (int frame = 0; frame < 60; ++frame) RogueEffects_OnFrame();
        assert(fabsf(Rogue_ModifyRunSpeed(&fighter, 10) - 12) < .001f);
        fighter.dmg.x1830_percent = 1;
        RogueEffects_OnParry(&fighter);
        assert(fighter.dmg.x1830_percent == 0);
        fighter.dmg.x1830_percent = 12;
        RogueEffects_OnParry(&fighter);
        assert(fighter.dmg.x1830_percent == 10);
        assert(RogueEffects_PreventShieldBreak(&fighter, 60));
        assert(fabsf(fighter.shield_health - 78) < .001f);
        assert(!RogueEffects_PreventShieldBreak(&fighter, 60));
        RogueEffects_BeginEncounter();
        assert(RogueEffects_PreventShieldBreak(&fighter, 60));
        g_rogue_run.stats.last_stand = true;
        g_rogue_run.stats.mirror = true;
        g_rogue_run.stats.piercing_shots = true;
        bool pierced = false, other_projectile = false;
        assert(RogueEffects_TryPierce(&fighter, It_Kind_Mario_Fire, &pierced));
        assert(!RogueEffects_TryPierce(&fighter, It_Kind_Mario_Fire, &pierced));
        assert(!RogueEffects_TryPierce(&enemy, It_Kind_Samus_Charge, &other_projectile));
        assert(!other_projectile);
        assert(!RogueEffects_TryPierce(&fighter, It_Kind_Link_HShot, &other_projectile));
        assert(!other_projectile);
        current_mode = GM_VS;
        assert(!RogueEffects_TryPierce(&fighter, It_Kind_Samus_Charge, &other_projectile));
        current_mode = GM_ROGUE;
        assert(RogueEffects_TryPierce(&fighter, It_Kind_Samus_Charge, &other_projectile));
        assert(!RogueEffects_CanMirror(&fighter));
        fighter.x221C_b2 = true;
        assert(RogueEffects_CanMirror(&fighter));
        enemy.x221C_b2 = true;
        assert(!RogueEffects_CanMirror(&enemy));
        current_mode = GM_VS;
        assert(!RogueEffects_CanMirror(&fighter));
        current_mode = GM_ROGUE;
        fighter.x221C_b2 = false;
        assert(!RogueEffects_TryLastStand(&enemy));
        assert(RogueEffects_TryLastStand(&fighter));
        RogueEffects_OnDeath(&fighter);
        assert(!RogueEffects_TryLastStand(&fighter));
        g_rogue_run.stats.ko_heal = 8;
        g_rogue_run.stats.smash_armor = 10;
        g_rogue_run.stats.hitstun_reduction = .10f;
        fighter.dmg.x1830_percent = 25;
        RogueEffects_OnKO(&fighter, &enemy);
        assert(fighter.dmg.x1830_percent == 17);
        enemy.is_sub_fighter = true;
        RogueEffects_OnKO(&fighter, &enemy);
        assert(fighter.dmg.x1830_percent == 17);
        enemy.is_sub_fighter = false;
        RogueEffects_OnKO(&fighter, &fighter);
        assert(fighter.dmg.x1830_percent == 17);
        fighter.smash_attrs.state = SmashState_Charging;
        assert(Rogue_ModifyArmor(&fighter, 5) == 15);
        fighter.smash_attrs.state = SmashState_Release;
        assert(Rogue_ModifyArmor(&fighter, 5) == 5);
        assert(Rogue_ModifyHitstun(&fighter, 40) == 36);
        RogueEffects_OnHit(&fighter, &enemy, 10);
        RogueEffects_OnDeath(&fighter);
        assert(g_rogue_run.effects.momentum_frames == 0);
        assert(g_rogue_run.effects.combo_stacks == 0);
        assert(!RogueEffects_PreventShieldBreak(&fighter, 60));
        fighter.dmg.x1830_percent = 10;
        current_mode = GM_VS;
        RogueEffects_OnKO(&fighter, &enemy);
        assert(fighter.dmg.x1830_percent == 10);
        fighter.smash_attrs.state = SmashState_Charging;
        assert(Rogue_ModifyArmor(&fighter, 5) == 5);
        assert(Rogue_ModifyHitstun(&fighter, 40) == 40);
        RogueEffects_BeginEncounter();
        assert(!RogueEffects_OnHit(&fighter, &enemy, 10));
        assert(!RogueEffects_PreventShieldBreak(&fighter, 60));
        RogueEffects_OnParry(&fighter);
        assert(fighter.dmg.x1830_percent == 10);
        current_mode = GM_ROGUE;
    }
    g_rogue_run.active = false;
    check(&fighter, false);
    puts("Roguelike hooks: exact modifiers and all-mode isolation passed");
    return 0;
}
