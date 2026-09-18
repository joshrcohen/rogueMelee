#include "rogue_state.h"

#include <melee/gm/types.h>
#include <melee/pl/forward.h>
#include <string.h>

RogueRun g_rogue_run;

void Rogue_ResetRun(void)
{
    memset(&g_rogue_run, 0, sizeof(g_rogue_run));
}

void Rogue_NewRun(CharacterKind kind, u32 seed)
{
    Rogue_ResetRun();
    if (kind < 0 || kind >= CKind_Playable_Count) {
        return;
    }
    g_rogue_run.active = true;
    g_rogue_run.seed = seed;
    RogueRng_Seed(&g_rogue_run.rng, seed);
    RogueRng_Seed(&g_rogue_run.route_rng, seed ^ 0x524F5554U);
    RogueRoute_Reset(&g_rogue_run.route);
    g_rogue_run.phase = ROGUE_PHASE_ROUTE;
    g_rogue_run.player_kind = kind;
    g_rogue_run.floor = 1;
    Rogue_ResetStats(&g_rogue_run.stats);
    RogueRoute_Prepare(&g_rogue_run.route, &g_rogue_run.route_rng,
                       g_rogue_run.floor);
}

bool Rogue_IsActive(void)
{
    return g_rogue_run.active;
}

bool Rogue_OnMatchEnd(const MatchEnd* result)
{
    if (!Rogue_IsActive() || g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER ||
        result == NULL)
    {
        return false;
    }
    /* Quitting, ties and malformed results cannot award a victory. */
    if ((result->outcome != OUTCOME_ELIMINATION &&
         !(result->is_teams && result->outcome == OUTCOME_TEAM_ELIMINATION)) ||
        (g_rogue_run.current_encounter.enemy_count > 1
             ? (!result->is_teams || result->n_team_winners != 1 || result->team_winners[0] != 0)
             : (result->is_teams || result->n_winners != 1 || result->winners[0] != 0)) ||
        result->player_standings[0].pkind != Gm_PKind_Human)
    {
        g_rogue_run.phase = ROGUE_PHASE_DEAD;
        g_rogue_run.active = false;
        return false;
    }
    ++g_rogue_run.wins;
    g_rogue_run.currency += g_rogue_run.current_encounter.type == ROGUE_ENCOUNTER_BOSS ? 100 :
                           g_rogue_run.current_encounter.type == ROGUE_ENCOUNTER_ELITE ? 60 : 30;
    if (g_rogue_run.wins == ROGUE_RUN_ENCOUNTERS) {
        g_rogue_run.phase = ROGUE_PHASE_COMPLETE;
        g_rogue_run.active = false;
        return false;
    }
    g_rogue_run.phase = ROGUE_PHASE_REWARD;
    Rogue_GenerateRewards();
    return true;
}
