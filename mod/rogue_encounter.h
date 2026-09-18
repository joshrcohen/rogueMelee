#ifndef MELEE_ROGUE_ENCOUNTER_H
#define MELEE_ROGUE_ENCOUNTER_H

#include "rogue_rng.h"
#include <melee/ft/forward.h>
#include <melee/mn/forward.h>

typedef enum RogueEncounterType {
    ROGUE_ENCOUNTER_NORMAL, ROGUE_ENCOUNTER_ELITE,
    ROGUE_ENCOUNTER_SWARM, ROGUE_ENCOUNTER_TEAM, ROGUE_ENCOUNTER_BOSS
} RogueEncounterType;

typedef struct RogueEnemy {
    CharacterKind kind;
    u8 cpu_level, stocks, costume;
    float attack_ratio, defense_ratio, model_scale;
    unsigned short start_damage;
    bool metal, invisible;
} RogueEnemy;

typedef struct RogueEncounter {
    RogueEncounterType type;
    RogueEnemy enemies[5];
    int enemy_count;
    int act, act_floor;
    const char* name;
    const char* modifier;
    /* Primary opponent used for encounter identity and themed rewards. */
    CharacterKind enemy_kind;
    StKind stage;
    u8 cpu_level;
    u8 stocks;
} RogueEncounter;

void Rogue_GenerateEncounter(RogueEncounter* encounter, RogueRng* rng,
                             int floor);
void Rogue_SetupEncounter(VsModeData* data, const RogueEncounter* encounter,
                          CharacterKind player_kind);

#endif
