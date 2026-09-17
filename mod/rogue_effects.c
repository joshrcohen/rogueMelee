static float rogue_max(float a, float b) { return a > b ? a : b; }
#include "rogue_effects.h"
#include "rogue_hooks.h"
#include "rogue_state.h"
#include <melee/ft/types.h>
#include <melee/it/forward.h>
#include <melee/gm/gm_1A3F.h>
#include <melee/pl/player.h>
#include <string.h>
#include <math.h>

void RogueEffects_BeginEncounter(void)
{
    memset(&g_rogue_run.effects, 0, sizeof(g_rogue_run.effects));
}

void RogueEffects_OnFrame(void)
{
    if (!Rogue_IsActive() || gm_GetCurrentGameMode() != GM_ROGUE) return;
    if (g_rogue_run.effects.combo_frames && --g_rogue_run.effects.combo_frames == 0)
        g_rogue_run.effects.combo_stacks = 0;
    if (g_rogue_run.effects.momentum_frames) --g_rogue_run.effects.momentum_frames;
}

/* Called once for a resolved damaging collision, never for phantom/shield hits. */
bool RogueEffects_OnHit(Fighter* attacker, Fighter* victim, float damage)
{
    if (!Rogue_IsRunPlayer(attacker) || !victim || victim == attacker || damage <= 0)
        return false;
    if (g_rogue_run.effects.combo_stacks < 5) ++g_rogue_run.effects.combo_stacks;
    g_rogue_run.effects.combo_frames = 120;
    g_rogue_run.effects.momentum_frames = 180;
    ++g_rogue_run.effects.hit_count;
    return g_rogue_run.stats.static_effect && g_rogue_run.effects.hit_count % 5 == 0;
}

void RogueEffects_OnParry(Fighter* fp)
{
    if (!Rogue_IsRunPlayer(fp) || g_rogue_run.stats.parry_heal <= 0) return;
    fp->dmg.x1830_percent = rogue_max(0, fp->dmg.x1830_percent - g_rogue_run.stats.parry_heal);
    Player_SetHPByIndex(fp->player_id, fp->is_sub_fighter, fp->dmg.x1830_percent);
}

bool RogueEffects_TryPierce(Fighter* owner, int kind, bool* used)
{
    if (!used || *used || !Rogue_IsRunPlayer(owner) || !g_rogue_run.stats.piercing_shots)
        return false;
    switch (kind) {
    case It_Kind_Mario_Fire: case It_Kind_DrMario_Vitamin:
    case It_Kind_Luigi_Fire: case It_Kind_Fox_Laser: case It_Kind_Falco_Laser:
    case It_Kind_Link_Arrow: case It_Kind_CLink_Arrow:
    case It_Kind_Link_Boomerang: case It_Kind_CLink_Boomerang:
    case It_Kind_Ness_PKFire: case It_Kind_Seak_NeedleThrow:
    case It_Kind_Yoshi_EggThrow: case It_Kind_Yoshi_Star:
    case It_Kind_Pikachu_TJolt_Ground: case It_Kind_Pikachu_TJolt_Air:
    case It_Kind_Pichu_TJolt_Ground: case It_Kind_Pichu_TJolt_Air:
    case It_Kind_Samus_Charge: case It_Kind_Samus_Missile:
    case It_Kind_IceClimber_Ice: case It_Kind_Mewtwo_ShadowBall:
    case It_Kind_Peach_ToadSpore:
        *used = true;
        return true;
    default:
        /* Held equipment, captures, explosions and persistent areas of damage
         * must retain their character-specific impact state transitions. */
        return false;
    }
}

bool RogueEffects_CanMirror(Fighter* fp)
{
    return Rogue_IsRunPlayer(fp) && g_rogue_run.stats.mirror && fp->x221C_b2;
}

bool RogueEffects_PreventShieldBreak(Fighter* fp, float capacity)
{
    if (!Rogue_IsRunPlayer(fp) || !g_rogue_run.stats.second_shell || g_rogue_run.effects.shell_used)
        return false;
    g_rogue_run.effects.shell_used = true;
    fp->shield_health = Rogue_ModifyShieldHealth(fp, capacity);
    return true;
}

void RogueEffects_OnKO(Fighter* killer, Fighter* victim)
{
    if (!Rogue_IsRunPlayer(killer) || !victim || victim->is_sub_fighter ||
        victim->player_id == killer->player_id || g_rogue_run.stats.ko_heal <= 0) return;
    killer->dmg.x1830_percent = rogue_max(0, killer->dmg.x1830_percent - g_rogue_run.stats.ko_heal);
    Player_SetHPByIndex(killer->player_id, killer->is_sub_fighter, killer->dmg.x1830_percent);
}

void RogueEffects_OnDeath(Fighter* fp)
{
    if (!Rogue_IsRunPlayer(fp)) return;
    /* Stock loss clears temporary momentum, but cannot refresh Second Shell. */
    g_rogue_run.effects.combo_frames = g_rogue_run.effects.combo_stacks = 0;
    g_rogue_run.effects.momentum_frames = 0;
}

bool RogueEffects_TryLastStand(Fighter* fp)
{
    if (!Rogue_IsRunPlayer(fp) || !g_rogue_run.stats.last_stand ||
        g_rogue_run.effects.last_stand_used) return false;
    g_rogue_run.effects.last_stand_used = true;
    RogueEffects_OnDeath(fp);
    return true;
}
