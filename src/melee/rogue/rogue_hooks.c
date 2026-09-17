static float rogue_max(float a, float b) { return a > b ? a : b; }
#include "rogue_hooks.h"
#include "rogue_state.h"
#include <melee/gm/gm_1A3F.h>
#include <melee/ft/types.h>
#include <melee/pl/player.h>
#include <math.h>
#include <melee/ft/kinds/ftCommon/forward.h>

bool Rogue_IsRunPlayer(const Fighter* fp)
{
    return Rogue_IsActive() && gm_GetCurrentGameMode() == GM_ROGUE &&
           fp != NULL && fp->player_id == 0 && !fp->is_sub_fighter &&
           Player_GetPlayerSlotType(0) == Gm_PKind_Human;
}

float Rogue_ModifyDamageDealt(const Fighter* fp, float value)
{
    return Rogue_IsRunPlayer(fp) ? value * g_rogue_run.stats.damage_dealt : value;
}

float Rogue_ModifyDamageReceived(const Fighter* fp, float value)
{
    return value > 0 && Rogue_IsRunPlayer(fp)
               ? value * g_rogue_run.stats.damage_received : value;
}

float Rogue_ModifyRunSpeed(const Fighter* fp, float value)
{
    return Rogue_IsRunPlayer(fp) ? value * (g_rogue_run.stats.run_speed + (g_rogue_run.effects.momentum_frames ? g_rogue_run.stats.momentum_bonus : 0)) : value;
}

float Rogue_ModifyShieldHealth(const Fighter* fp, float value)
{
    return Rogue_IsRunPlayer(fp) ? value * g_rogue_run.stats.shield_health : value;
}

float Rogue_ModifyShieldRegen(const Fighter* fp, float value)
{
    return Rogue_IsRunPlayer(fp) ? value * (1 + g_rogue_run.stats.shield_regen_bonus) : value;
}

/* Called only immediately after rebuilding instance attributes from the DAT.
 * This avoids compounding bonuses on respawn, transformation or item changes. */
void Rogue_ApplyMovementStats(Fighter* fp)
{
    RogueStats* stats = &g_rogue_run.stats;
    float jump;
    if (!Rogue_IsRunPlayer(fp)) return;
    jump = sqrtf(1 + stats->jump_height_bonus);
    fp->co_attrs.jump_v_initial_velocity *= jump;
    fp->co_attrs.hop_v_initial_velocity *= jump;
    fp->co_attrs.air_drift_stick_mul *= 1 + stats->air_control_bonus;
    fp->co_attrs.aerial_drift_base *= 1 + stats->air_control_bonus;
    fp->co_attrs.air_drift_max *= 1 + stats->air_control_bonus;
    fp->co_attrs.fast_fall_velocity *= 1 + stats->fast_fall_bonus;
    fp->co_attrs.ground_friction /= 1 + stats->slide_bonus;
    fp->co_attrs.max_jumps += stats->extra_jumps;
}

/* Projectile bonuses use the impact target but never inherit the owner's
 * current animation: a laser already in flight is not a smash or aerial. */
float Rogue_ModifyAttackDamage(const Fighter* attacker, const Fighter* victim,
                               float value, bool projectile)
{
    RogueStats* stats = &g_rogue_run.stats;
    float bonus = g_rogue_run.effects.combo_stacks * stats->combo_bonus;
    if (!Rogue_IsRunPlayer(attacker)) return value;
    if (victim && victim->dmg.x1830_percent > 100) bonus += stats->executioner_bonus;
    if (!projectile) {
        if (attacker->motion_id >= ftCo_MS_AttackAirN &&
            attacker->motion_id <= ftCo_MS_AttackAirLw) bonus += stats->aerial_damage_bonus;
        if (attacker->motion_id >= ftCo_MS_AttackS4Hi &&
            attacker->motion_id <= ftCo_MS_AttackLw4) bonus += stats->smash_damage_bonus;
    }
    return value * (stats->damage_dealt + bonus);
}

float Rogue_ModifyKnockback(const Fighter* attacker, const Fighter* victim, float value)
{
    if (Rogue_IsRunPlayer(attacker)) value *= 1 + g_rogue_run.stats.knockback_dealt_bonus;
    if (Rogue_IsRunPlayer(victim)) value *= 1 - g_rogue_run.stats.knockback_resistance;
    return value;
}

float Rogue_ModifyShieldStun(const Fighter* fp, float frames)
{
    return Rogue_IsRunPlayer(fp) ? rogue_max(1, frames * (1 - g_rogue_run.stats.shield_stun_reduction)) : frames;
}

float Rogue_ModifyArmor(const Fighter* fp, float armor)
{
    return Rogue_IsRunPlayer(fp) && fp->smash_attrs.state == SmashState_Charging
        ? armor + g_rogue_run.stats.smash_armor : armor;
}

float Rogue_ModifyHitstun(const Fighter* fp, float frames)
{
    return Rogue_IsRunPlayer(fp) ? rogue_max(1, frames * (1 - g_rogue_run.stats.hitstun_reduction)) : frames;
}
