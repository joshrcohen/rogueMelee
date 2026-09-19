#ifndef MELEE_ROGUE_REWARDS_H
#define MELEE_ROGUE_REWARDS_H

#include <Runtime/platform.h>
#include <stddef.h>

typedef enum RogueRarity {
    ROGUE_RARITY_COMMON, ROGUE_RARITY_RARE,
    ROGUE_RARITY_EPIC, ROGUE_RARITY_LEGENDARY
} RogueRarity;

typedef enum RogueRewardType {
    ROGUE_REWARD_ATTACK,
    ROGUE_REWARD_DEFENSE,
    ROGUE_REWARD_MOVEMENT,
    ROGUE_REWARD_SHIELD,
    ROGUE_REWARD_ABILITY,
    ROGUE_REWARD_AIR_CONTROL,
    ROGUE_REWARD_JUMP_HEIGHT,
    ROGUE_REWARD_EXTRA_JUMP,
    ROGUE_REWARD_FAST_FALL,
    ROGUE_REWARD_SLIDE,
    ROGUE_REWARD_SHIELD_REGEN,
    ROGUE_REWARD_LAUNCH_POWER,
    ROGUE_REWARD_ANCHOR,
    ROGUE_REWARD_EXECUTIONER,
    ROGUE_REWARD_AERIAL_ACE,
    ROGUE_REWARD_SMASH_MASTER,
    ROGUE_REWARD_BRACE,
    ROGUE_REWARD_COMBO_ENGINE,
    ROGUE_REWARD_MOMENTUM,
    ROGUE_REWARD_STATIC,
    ROGUE_REWARD_GLASS_CANNON,
    ROGUE_REWARD_PARRY_HEAL,
    ROGUE_REWARD_SECOND_SHELL,
    ROGUE_REWARD_BLOODLUST,
    ROGUE_REWARD_HEAVY_ARMOR,
    ROGUE_REWARD_RECOVERY_WINDOW,
    ROGUE_REWARD_LAST_STAND,
    ROGUE_REWARD_MIRROR,
    ROGUE_REWARD_PIERCING_SHOTS,
    ROGUE_REWARD_COUNT,
} RogueRewardType;

typedef struct RogueStats {
    float damage_dealt;
    float damage_received;
    float run_speed;
    float shield_health;
    float air_control_bonus, jump_height_bonus, fast_fall_bonus;
    float slide_bonus, shield_regen_bonus;
    int extra_jumps;
    float knockback_dealt_bonus, knockback_resistance;
    float executioner_bonus, aerial_damage_bonus, smash_damage_bonus;
    float shield_stun_reduction;
    float combo_bonus, momentum_bonus, parry_heal;
    bool static_effect, second_shell, last_stand, mirror;
    bool piercing_shots;
    float ko_heal, smash_armor, hitstun_reduction;
} RogueStats;

typedef struct RogueReward {
    RogueRewardType type;
    const char* name;
    const char* description;
    float magnitude;
    int ability_id;
    RogueRarity rarity;
} RogueReward;

const char* Rogue_RarityName(RogueRarity rarity);
void Rogue_DescribeReward(const RogueReward* reward, char* text, size_t size);

void Rogue_GenerateRewards(void);
bool Rogue_SelectReward(int index);
bool Rogue_BeginOpeningCamp(void);
bool Rogue_BeginCamp(void);
bool Rogue_BuySupply(int index);
bool Rogue_Rest(int choice);
void Rogue_LeaveCamp(void);
void Rogue_ResetStats(RogueStats* stats);

#endif
