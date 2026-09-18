#ifndef MELEE_ROGUE_STATE_H
#define MELEE_ROGUE_STATE_H

#include "rogue_encounter.h"
#include "rogue_route.h"
#include "rogue_rewards.h"
#include "rogue_ability.h"
#include <melee/gm/forward.h>

#define ROGUE_ACTS 3
#define ROGUE_FLOORS_PER_ACT 5
#define ROGUE_RUN_ENCOUNTERS (ROGUE_ACTS * ROGUE_FLOORS_PER_ACT)

typedef enum RoguePhase {
    ROGUE_PHASE_NONE,
    ROGUE_PHASE_ROUTE,
    ROGUE_PHASE_ENCOUNTER,
    ROGUE_PHASE_REWARD,
    ROGUE_PHASE_SHOP,
    ROGUE_PHASE_REST,
    ROGUE_PHASE_COMPLETE,
    ROGUE_PHASE_DEAD,
} RoguePhase;

typedef struct RogueRun {
    bool active;
    u32 seed;
    RogueRng rng;
    RogueRng route_rng;
    RoguePhase phase;
    CharacterKind player_kind;
    u8 player_costume;
    int floor;
    int wins;
    int currency;
    RogueStats stats;
    struct {
        unsigned combo_frames, momentum_frames, hit_count, combo_stacks;
        bool shell_used, last_stand_used;
    } effects;
    RogueAbilityID ability[ROGUE_ABILITY_SLOTS];
    RogueReward current_rewards[3];
    RogueReward shop_rewards[2];
    int shop_prices[2];
    bool shop_sold[2];
    int camp_floor;
    bool reward_pending;
    RogueRoute route;
    RogueEncounter current_encounter;
} RogueRun;

extern RogueRun g_rogue_run;

void Rogue_ResetRun(void);
void Rogue_NewRun(CharacterKind kind, u32 seed);
bool Rogue_IsActive(void);
/* Returns true only when another encounter should launch. */
bool Rogue_OnMatchEnd(const MatchEnd* result);

#endif
