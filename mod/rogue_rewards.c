#include "rogue_format.h"
#include "rogue_rewards.h"
#include "rogue_state.h"
#include <stdio.h>
#include <string.h>

static const RogueReward rewards[] = {
    { ROGUE_REWARD_ATTACK, "Heavy Hands", "+20% damage dealt.", 0.20f },
    { ROGUE_REWARD_DEFENSE, "Thick Skin", "-20% damage received. Minimum 20%.", 0.20f },
    { ROGUE_REWARD_MOVEMENT, "Fleet Footed", "+20% dash and run speed.", 0.20f },
    { ROGUE_REWARD_SHIELD, "Reinforced Shield", "+30% shield capacity.", 0.30f },
    { ROGUE_REWARD_AIR_CONTROL, "Air Control", "+15% air movement.", 0.15f },
    { ROGUE_REWARD_JUMP_HEIGHT, "Moon Shoes", "+12% jump height.", 0.12f },
    { ROGUE_REWARD_EXTRA_JUMP, "Double Jump+", "One additional aerial jump.", 1.0f },
    { ROGUE_REWARD_FAST_FALL, "Fast Fall+", "+15% fast-fall speed.", 0.15f },
    { ROGUE_REWARD_SLIDE, "Wave Rider", "Longer ground slides through lower friction.", 0.15f },
    { ROGUE_REWARD_SHIELD_REGEN, "Quick Recharge", "+30% shield regeneration.", 0.30f },
    { ROGUE_REWARD_LAUNCH_POWER, "Launch Power", "+12% knockback dealt.", 0.12f },
    { ROGUE_REWARD_ANCHOR, "Anchor", "-12% knockback received.", 0.12f },
    { ROGUE_REWARD_EXECUTIONER, "Executioner", "+25% damage against enemies above 100%.", 0.25f },
    { ROGUE_REWARD_AERIAL_ACE, "Aerial Ace", "+18% aerial attack damage.", 0.18f },
    { ROGUE_REWARD_SMASH_MASTER, "Smash Master", "+20% smash attack damage.", 0.20f },
    { ROGUE_REWARD_BRACE, "Brace", "-15% shield stun.", 0.15f },
    { ROGUE_REWARD_COMBO_ENGINE, "Combo Engine", "Each hit within two seconds adds 3% damage, up to five stacks.", .03f },
    { ROGUE_REWARD_MOMENTUM, "Momentum", "Hits grant 3% run speed for three seconds.", .03f },
    { ROGUE_REWARD_STATIC, "Static", "Every fifth successful hit applies electricity.", 1 },
    { ROGUE_REWARD_GLASS_CANNON, "Glass Cannon", "+35% damage dealt, +20% damage received.", .35f },
    { ROGUE_REWARD_PARRY_HEAL, "Parry Heal", "Perfect shields heal 2% damage.", 2 },
    { ROGUE_REWARD_SECOND_SHELL, "Second Shell", "Prevent one shield break per encounter and refill your shield.", 1 },
    { ROGUE_REWARD_BLOODLUST, "Bloodlust", "KOing an opponent heals 8% damage.", 8 },
    { ROGUE_REWARD_HEAVY_ARMOR, "Heavy Armor", "Gain 10 knockback armor while charging a smash.", 10 },
    { ROGUE_REWARD_RECOVERY_WINDOW, "Recovery Window", "Reduce hitstun by 10%.", .10f },
    { ROGUE_REWARD_LAST_STAND, "Last Stand", "Survive one blast-zone KO per encounter. Return on the revival platform with damage and stocks preserved.", 1 },
    { ROGUE_REWARD_MIRROR, "Mirror", "Perfect shields reflect projectiles across the full shield and parry window at full damage.", 1 },
    { ROGUE_REWARD_PIERCING_SHOTS, "Piercing Shots", "Traveling projectiles pass through their first damaging hit. Shields and terrain still stop them normally.", 1 },
};
#define REWARD_POOL_COUNT (sizeof(rewards) / sizeof(rewards[0]))

static int availableRewards(int* pool)
{
    int count = 0;
    for (int i = 0; i < REWARD_POOL_COUNT; ++i) {
        if (rewards[i].type == ROGUE_REWARD_STATIC && g_rogue_run.stats.static_effect) continue;
        if (rewards[i].type == ROGUE_REWARD_SECOND_SHELL && g_rogue_run.stats.second_shell) continue;
        if (rewards[i].type == ROGUE_REWARD_LAST_STAND && g_rogue_run.stats.last_stand) continue;
        if (rewards[i].type == ROGUE_REWARD_MIRROR && g_rogue_run.stats.mirror) continue;
        if (rewards[i].type == ROGUE_REWARD_PIERCING_SHOTS && g_rogue_run.stats.piercing_shots) continue;
        pool[count++] = i;
    }
    return count;
}

const char* Rogue_RarityName(RogueRarity rarity)
{
    static const char* names[] = { "Common", "Rare", "Epic", "Legendary" };
    return rarity >= ROGUE_RARITY_COMMON && rarity <= ROGUE_RARITY_LEGENDARY ? names[rarity] : "Common";
}

static RogueRarity rollRarity(bool elite, bool boss)
{
    unsigned roll = RogueRng_Bounded(&g_rogue_run.rng, 100);
    if (boss) return roll < 65 ? ROGUE_RARITY_RARE : roll < 95 ? ROGUE_RARITY_EPIC : ROGUE_RARITY_LEGENDARY;
    if (elite) return roll < 20 ? ROGUE_RARITY_COMMON : roll < 75 ? ROGUE_RARITY_RARE : roll < 97 ? ROGUE_RARITY_EPIC : ROGUE_RARITY_LEGENDARY;
    return roll < 70 ? ROGUE_RARITY_COMMON : roll < 95 ? ROGUE_RARITY_RARE : ROGUE_RARITY_EPIC;
}

static void scaleReward(RogueReward* reward, RogueRarity rarity)
{
    static const float multipliers[] = { 1.0f, 1.5f, 2.0f, 3.0f };
    reward->rarity = rarity;
    reward->magnitude *= multipliers[rarity];
    if (reward->type == ROGUE_REWARD_MIRROR) reward->magnitude = 1;
    if (reward->type == ROGUE_REWARD_PIERCING_SHOTS) reward->magnitude = 1;
    if (reward->type == ROGUE_REWARD_EXTRA_JUMP || reward->type == ROGUE_REWARD_STATIC || reward->type == ROGUE_REWARD_SECOND_SHELL || reward->type == ROGUE_REWARD_LAST_STAND) reward->magnitude = 1;
}

void Rogue_DescribeReward(const RogueReward* reward, char* text, size_t size)
{
    const char* stat = NULL;
    switch (reward->type) {
    case ROGUE_REWARD_ATTACK: stat = "damage dealt"; break;
    case ROGUE_REWARD_DEFENSE: stat = "damage received (minimum 20%)"; break;
    case ROGUE_REWARD_MOVEMENT: stat = "dash and run speed"; break;
    case ROGUE_REWARD_SHIELD: stat = "shield capacity"; break;
    case ROGUE_REWARD_AIR_CONTROL: stat = "air acceleration and speed"; break;
    case ROGUE_REWARD_JUMP_HEIGHT: stat = "jump height"; break;
    case ROGUE_REWARD_FAST_FALL: stat = "fast-fall speed"; break;
    case ROGUE_REWARD_SHIELD_REGEN: stat = "shield regeneration"; break;
    case ROGUE_REWARD_LAUNCH_POWER: stat = "knockback dealt"; break;
    case ROGUE_REWARD_ANCHOR: stat = "knockback received (minimum 40%)"; break;
    case ROGUE_REWARD_EXECUTIONER: stat = "damage against enemies above 100%"; break;
    case ROGUE_REWARD_AERIAL_ACE: stat = "aerial attack damage"; break;
    case ROGUE_REWARD_SMASH_MASTER: stat = "smash attack damage"; break;
    case ROGUE_REWARD_BRACE: stat = "shield stun (minimum 40%)"; break;
    case ROGUE_REWARD_COMBO_ENGINE:
        snprintf(text, size, "Each hit within two seconds adds %.1f%% damage, up to five stacks.", reward->magnitude * 100); return;
    case ROGUE_REWARD_MOMENTUM:
        snprintf(text, size, "Hits grant +%.1f%% dash/run speed for three seconds. Further hits refresh the timer.", reward->magnitude * 100); return;
    case ROGUE_REWARD_GLASS_CANNON:
        snprintf(text, size, "+%.0f%% damage dealt and +20%% damage received for this run.", reward->magnitude * 100); return;
    case ROGUE_REWARD_PARRY_HEAL:
        snprintf(text, size, "Perfect shields heal %.0f%% damage.", reward->magnitude); return;
    case ROGUE_REWARD_BLOODLUST:
        snprintf(text, size, "KOing an opponent heals %.0f%% damage.", reward->magnitude); return;
    case ROGUE_REWARD_HEAVY_ARMOR:
        snprintf(text, size, "Gain %.0f knockback armor while charging a smash.", reward->magnitude); return;
    case ROGUE_REWARD_RECOVERY_WINDOW:
        snprintf(text, size, "Reduce hitstun by %.0f%% (maximum 40%% reduction).", reward->magnitude * 100); return;
    case ROGUE_REWARD_SLIDE:
        snprintf(text, size, "Ground friction divided by %.2f for longer slides.", 1 + reward->magnitude);
        return;
    case ROGUE_REWARD_ABILITY: {
        const RogueAbilityDefinition* next = Rogue_GetAbility(reward->ability_id);
        const RogueAbilityDefinition* old = NULL;
        static const char* slots[] = { "Neutral-B", "Side-B", "Up-B", "Down-B" };
        if (next) {
            RogueAbilityID id = g_rogue_run.ability[next->native_slot];
            if (id == ROGUE_ABILITY_NATIVE)
                id = Rogue_AbilityForOpponent(g_rogue_run.player_kind, next->native_slot);
            old = Rogue_GetAbility(id);
            snprintf(text, size, "%s: replace %s with %s. Other slots stay equipped.",
                     slots[next->native_slot], old ? old->name : "native special", next->name);
            return;
        }
        break;
    }
    default: break;
    }
    if (stat) snprintf(text, size, "%c%.0f%% %s for this run.",
                       (reward->type == ROGUE_REWARD_DEFENSE || reward->type == ROGUE_REWARD_ANCHOR || reward->type == ROGUE_REWARD_BRACE) ? '-' : '+', reward->magnitude * 100, stat);
    else snprintf(text, size, "%s", reward->description);
}

void Rogue_ResetStats(RogueStats* stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->damage_dealt = 1.0f;
    stats->damage_received = 1.0f;
    stats->run_speed = 1.0f;
    stats->shield_health = 1.0f;
}

void Rogue_GenerateRewards(void)
{
    int pool[REWARD_POOL_COUNT];
    int i;
    int count = availableRewards(pool);
    for (i = 0; i < 3; ++i) {
        int j = i + RogueRng_Bounded(&g_rogue_run.rng, count - i);
        int swap = pool[i];
        pool[i] = pool[j];
        pool[j] = swap;
        g_rogue_run.current_rewards[i] = rewards[pool[i]];
        scaleReward(&g_rogue_run.current_rewards[i], rollRarity(
            g_rogue_run.current_encounter.type == ROGUE_ENCOUNTER_ELITE,
            g_rogue_run.current_encounter.type == ROGUE_ENCOUNTER_BOSS));
    }
    /*
     * Any actual fighter defeated in the encounter may donate a special.
     * This matters for Tag Team encounters, where the secondary opponent used
     * to be invisible to the ability reward system.
     */
    if (g_rogue_run.current_encounter.enemy_count > 0) {
        static const char* descriptions[4] = {
            "Replace Neutral-B with this fighter's neutral special.",
            "Replace Side-B with this fighter's side special.",
            "Replace Up-B with this fighter's recovery special.",
            "Replace Down-B with this fighter's down special."
        };
        int enemy_first = RogueRng_Bounded(
            &g_rogue_run.rng, g_rogue_run.current_encounter.enemy_count);
        bool offered = false;
        for (int enemy_i = 0;
             enemy_i < g_rogue_run.current_encounter.enemy_count && !offered;
             ++enemy_i) {
            CharacterKind source =
                g_rogue_run.current_encounter.enemies[
                    (enemy_first + enemy_i) %
                    g_rogue_run.current_encounter.enemy_count].kind;
            int first;
            if (source < 0 || source >= CKind_Playable_Count ||
                source == g_rogue_run.player_kind)
                continue;
            first = RogueRng_Bounded(&g_rogue_run.rng, 4);
            for (i = 0; i < 4; ++i) {
                int slot = (first + i) % 4;
                RogueAbilityID id = Rogue_AbilityForOpponent(source, slot);
                const RogueAbilityDefinition* def = Rogue_GetAbility(id);
                if (!def || id == g_rogue_run.ability[slot]) continue;
                g_rogue_run.current_rewards[0] = (RogueReward) {
                    ROGUE_REWARD_ABILITY, def->name, descriptions[slot], 0, id,
                    g_rogue_run.current_rewards[0].rarity
                };
                offered = true;
                break;
            }
        }
    }
    g_rogue_run.reward_pending = true;
}

static bool applyReward(const RogueReward* reward)
{
    RogueStats* stats = &g_rogue_run.stats;
    /* All rewards stack additively around a base multiplier of one. */
    switch (reward->type) {
    case ROGUE_REWARD_ATTACK: stats->damage_dealt += reward->magnitude; break;
    case ROGUE_REWARD_DEFENSE:
        stats->damage_received -= reward->magnitude;
        if (stats->damage_received < 0.20f) stats->damage_received = 0.20f;
        break;
    case ROGUE_REWARD_MOVEMENT: stats->run_speed += reward->magnitude; break;
    case ROGUE_REWARD_SHIELD: stats->shield_health += reward->magnitude; break;
    case ROGUE_REWARD_AIR_CONTROL: stats->air_control_bonus += reward->magnitude; break;
    case ROGUE_REWARD_JUMP_HEIGHT: stats->jump_height_bonus += reward->magnitude; break;
    case ROGUE_REWARD_EXTRA_JUMP: stats->extra_jumps += 1; break;
    case ROGUE_REWARD_FAST_FALL: stats->fast_fall_bonus += reward->magnitude; break;
    case ROGUE_REWARD_SLIDE: stats->slide_bonus += reward->magnitude; break;
    case ROGUE_REWARD_SHIELD_REGEN: stats->shield_regen_bonus += reward->magnitude; break;
    case ROGUE_REWARD_LAUNCH_POWER: stats->knockback_dealt_bonus += reward->magnitude; break;
    case ROGUE_REWARD_ANCHOR:
        stats->knockback_resistance += reward->magnitude;
        if (stats->knockback_resistance > .60f) stats->knockback_resistance = .60f;
        break;
    case ROGUE_REWARD_EXECUTIONER: stats->executioner_bonus += reward->magnitude; break;
    case ROGUE_REWARD_AERIAL_ACE: stats->aerial_damage_bonus += reward->magnitude; break;
    case ROGUE_REWARD_SMASH_MASTER: stats->smash_damage_bonus += reward->magnitude; break;
    case ROGUE_REWARD_BRACE:
        stats->shield_stun_reduction += reward->magnitude;
        if (stats->shield_stun_reduction > .60f) stats->shield_stun_reduction = .60f;
        break;
    case ROGUE_REWARD_LAST_STAND: stats->last_stand = true; break;
    case ROGUE_REWARD_MIRROR: stats->mirror = true; break;
    case ROGUE_REWARD_PIERCING_SHOTS: stats->piercing_shots = true; break;
    case ROGUE_REWARD_BLOODLUST: stats->ko_heal += reward->magnitude; break;
    case ROGUE_REWARD_HEAVY_ARMOR: stats->smash_armor += reward->magnitude; break;
    case ROGUE_REWARD_RECOVERY_WINDOW:
        stats->hitstun_reduction += reward->magnitude;
        if (stats->hitstun_reduction > .40f) stats->hitstun_reduction = .40f;
        break;
    case ROGUE_REWARD_COMBO_ENGINE: stats->combo_bonus += reward->magnitude; break;
    case ROGUE_REWARD_MOMENTUM: stats->momentum_bonus += reward->magnitude; break;
    case ROGUE_REWARD_STATIC: stats->static_effect = true; break;
    case ROGUE_REWARD_GLASS_CANNON:
        stats->damage_dealt += reward->magnitude;
        stats->damage_received += .20f; break;
    case ROGUE_REWARD_PARRY_HEAL: stats->parry_heal += reward->magnitude; break;
    case ROGUE_REWARD_SECOND_SHELL: stats->second_shell = true; break;
    case ROGUE_REWARD_ABILITY: {
        const RogueAbilityDefinition* def = Rogue_GetAbility(reward->ability_id);
        if (!def) return false;
        g_rogue_run.ability[def->native_slot] = def->id;
        break;
    }
    default: return false;
    }
    return true;
}

bool Rogue_SelectReward(int index)
{
    if (!Rogue_IsActive() || g_rogue_run.phase != ROGUE_PHASE_REWARD ||
        !g_rogue_run.reward_pending || index < 0 || index >= 3) return false;
    if (!applyReward(&g_rogue_run.current_rewards[index])) return false;
    g_rogue_run.reward_pending = false;
    ++g_rogue_run.floor;

    if (((g_rogue_run.floor - 1) % ROGUE_FLOORS_PER_ACT) + 1 ==
        ROGUE_FLOORS_PER_ACT)
    {
        if (!RogueRoute_UseBoss(&g_rogue_run.route,
                                &g_rogue_run.current_encounter))
            return false;
        g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
    } else {
        if (!RogueRoute_Prepare(&g_rogue_run.route,
                                &g_rogue_run.route_rng,
                                g_rogue_run.floor))
            return false;
        g_rogue_run.phase = ROGUE_PHASE_ROUTE;
    }
    return true;
}

bool Rogue_BeginCamp(void)
{
    int first, second, pool[REWARD_POOL_COUNT], count;
    if (!Rogue_IsActive() || g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER ||
        g_rogue_run.current_encounter.act_floor != ROGUE_FLOORS_PER_ACT ||
        g_rogue_run.camp_floor == g_rogue_run.floor) return false;
    count = availableRewards(pool);
    first = RogueRng_Bounded(&g_rogue_run.rng, count);
    second = (first + 1 + RogueRng_Bounded(&g_rogue_run.rng, count - 1)) % count;
    g_rogue_run.shop_rewards[0] = rewards[pool[first]];
    g_rogue_run.shop_rewards[1] = rewards[pool[second]];
    for (int i = 0; i < 2; ++i) {
        scaleReward(&g_rogue_run.shop_rewards[i], rollRarity(true, false));
        g_rogue_run.shop_sold[i] = false;
        g_rogue_run.shop_prices[i] = 45 + 10 * g_rogue_run.current_encounter.act +
            20 * g_rogue_run.shop_rewards[i].rarity;
    }
    g_rogue_run.camp_floor = g_rogue_run.floor;
    g_rogue_run.phase = ROGUE_PHASE_REST;
    return true;
}

bool Rogue_BuySupply(int index)
{
    if (!Rogue_IsActive() || g_rogue_run.phase != ROGUE_PHASE_SHOP ||
        index < 0 || index >= 2 || g_rogue_run.shop_sold[index] ||
        g_rogue_run.currency < g_rogue_run.shop_prices[index]) return false;
    if (!applyReward(&g_rogue_run.shop_rewards[index])) return false;
    g_rogue_run.currency -= g_rogue_run.shop_prices[index];
    g_rogue_run.shop_sold[index] = true;
    return true;
}

bool Rogue_Rest(int choice)
{
    if (!Rogue_IsActive() || g_rogue_run.phase != ROGUE_PHASE_REST ||
        choice < 0 || choice > 1) return false;
    /* Damage resets every fight, so resting improves the next fights rather
     * than offering healing which would have no effect. */
    if (choice == 0) g_rogue_run.stats.shield_health += 0.15f;
    else g_rogue_run.stats.run_speed += 0.10f;
    g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
    return true;
}

void Rogue_LeaveCamp(void)
{
    if (Rogue_IsActive() && (g_rogue_run.phase == ROGUE_PHASE_SHOP ||
                            g_rogue_run.phase == ROGUE_PHASE_REST))
        g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
}
