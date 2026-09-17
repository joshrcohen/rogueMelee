#include <melee/rogue/rogue_state.h>
#include <melee/gm/types.h>
#include <melee/pl/forward.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* This target covers numerical progression; ability assets need the engine. */
const RogueAbilityDefinition* Rogue_GetAbility(RogueAbilityID id) { return NULL; }
RogueAbilityID Rogue_AbilityForOpponent(CharacterKind kind, RogueAbilitySlot slot)
{ return ROGUE_ABILITY_NATIVE; }

/* Only engine dependency of the encounter builder. The full game's defaults
 * and disc/scene loading still require the integration checklist. */
void gm_InitVsMode(VsModeData* data)
{
    int i;
    memset(data, 0, sizeof(*data));
    data->start.rules.game_speed = 1.0f;
    for (i = 0; i < GM_MAX_PLAYERS; ++i) {
        data->start.players[i].ckind = ChKind_None;
        data->start.players[i].slot_type = Gm_PKind_NA;
        data->start.players[i].cpu_kind = 4;
        data->start.players[i].attack_ratio = 1.0f;
        data->start.players[i].defense_ratio = 1.0f;
        data->start.players[i].model_scale = 1.0f;
    }
}

static MatchEnd win(void)
{
    MatchEnd result = { 0 };
    result.outcome = OUTCOME_ELIMINATION;
    result.n_winners = 1;
    result.winners[0] = 0;
    result.player_standings[0].pkind = Gm_PKind_Human;
    return result;
}

int main(void)
{
    RogueEncounter route[ROGUE_RUN_ENCOUNTERS];
    RogueRng rng;
    MatchEnd result = win();
    VsModeData data;
    int i, pass;

    RogueRng_Seed(&rng, 0);
    assert(RogueRng_Next(&rng) == 0xE220A839U);
    assert(RogueRng_Next(&rng) == 0x6E789E6AU);
    assert(RogueRng_Bounded(&rng, 0) == 0);

    Rogue_ResetRun();
    assert(!Rogue_IsActive());
    assert(!Rogue_OnMatchEnd(&result));
    Rogue_NewRun(CKind_MasterH, 0);
    assert(!Rogue_IsActive());

    /* Replay the full three-act route, including every team and boss result. */
    for (pass = 0; pass < 2; ++pass) {
        Rogue_NewRun(CKind_Mario, 0);
        for (i = 0; i < ROGUE_RUN_ENCOUNTERS; ++i) {
            RogueEncounter e = g_rogue_run.current_encounter;
            result = win();
            if (e.enemy_count > 1) {
                result.is_teams = true;
                result.n_team_winners = 1;
                result.team_winners[0] = 0;
            }
            assert(g_rogue_run.floor == i + 1);
            assert(g_rogue_run.wins == i);
            assert(Rogue_IsActive());
            if (pass == 0) {
                route[i] = e;
            } else {
                assert(memcmp(&e, &route[i], sizeof(e)) == 0);
            }
            assert(e.enemy_kind >= 0 && (e.enemy_kind < CKind_Playable_Count ||
                   (i == 14 && e.enemy_kind == CKind_MasterH)));
            assert(e.cpu_level >= 4 && e.cpu_level <= 9);
            assert(e.stocks == 3);
            memset(&data, 0xFF, sizeof(data));
            Rogue_SetupEncounter(&data, &e, CKind_Mario);
            assert(data.start.rules.match_kind == MatchKind_Stock);
            assert(data.start.rules.is_stock);
            if (i == 14) {
                assert(e.enemy_kind == CKind_MasterH && e.enemy_count == 1);
                assert(data.start.players[1].hp == 300 && data.start.players[1].xC_b7);
                assert(data.start.players[1].stocks == 1 && data.start.players[1].model_scale == 1);
            }
            assert(data.start.rules.is_teams == (e.enemy_count > 1));
            assert(e.act == i / 5 + 1 && e.act_floor == i % 5 + 1);
            assert((e.type == ROGUE_ENCOUNTER_BOSS) == (e.act_floor == 5));
            for (int p = 1; p <= e.enemy_count; ++p) {
                assert(data.start.players[p].slot_type == Gm_PKind_Cpu);
                assert(data.start.players[p].team == 1);
                assert(data.start.players[p].stocks == e.enemies[p - 1].stocks);
                assert(data.start.players[p].model_scale == e.enemies[p - 1].model_scale);
            }
            assert(!data.start.rules.timer_enabled);
            assert(data.start.rules.item_freq == -1);
            assert(data.start.players[0].slot_type == Gm_PKind_Human);
            assert(data.start.players[0].ckind == CKind_Mario);
            assert(data.start.players[0].damage == 0);
            assert(data.start.players[1].damage == 0);
            assert(data.start.players[1].slot_type == Gm_PKind_Cpu);
            assert(data.start.players[1].ckind == e.enemy_kind);
            assert(data.start.players[5].slot_type == Gm_PKind_NA);
            assert(Rogue_OnMatchEnd(&result) == (i < ROGUE_RUN_ENCOUNTERS - 1));
            if (i < ROGUE_RUN_ENCOUNTERS - 1) {
                assert(g_rogue_run.phase == ROGUE_PHASE_REWARD);
                assert(!Rogue_SelectReward(-1));
                assert(!Rogue_SelectReward(3));
                assert(g_rogue_run.current_rewards[0].type != g_rogue_run.current_rewards[1].type);
                assert(g_rogue_run.current_rewards[1].type != g_rogue_run.current_rewards[2].type);
                assert(g_rogue_run.current_rewards[0].type != g_rogue_run.current_rewards[2].type);
                assert(Rogue_SelectReward(i % 3));
                assert(!Rogue_SelectReward(i % 3));
                if (Rogue_BeginCamp()) {
                    assert(!Rogue_BeginCamp());
                    if (g_rogue_run.current_encounter.act == 2) {
                        float speed = g_rogue_run.stats.run_speed;
                        assert(Rogue_Rest(1));
                        assert(g_rogue_run.stats.run_speed > speed);
                        assert(!Rogue_Rest(1));
                    } else {
                        int gold = g_rogue_run.currency;
                        assert(!Rogue_BuySupply(0));
                        g_rogue_run.phase = ROGUE_PHASE_SHOP;
                        assert(Rogue_BuySupply(0));
                        assert(g_rogue_run.currency == gold - g_rogue_run.shop_prices[0]);
                        assert(!Rogue_BuySupply(0));
                        Rogue_LeaveCamp();
                    }
                    assert(g_rogue_run.phase == ROGUE_PHASE_ENCOUNTER);
                    assert(!Rogue_BeginCamp());
                }
            }
        }
        assert(!Rogue_IsActive());
        assert(g_rogue_run.phase == ROGUE_PHASE_COMPLETE);
        assert(g_rogue_run.wins == ROGUE_RUN_ENCOUNTERS);
        assert(!Rogue_OnMatchEnd(&result));
        assert(g_rogue_run.wins == ROGUE_RUN_ENCOUNTERS);
    }

    for (i = 0; i < 4; ++i) {
        Rogue_NewRun(CKind_Mario, 123);
        result = win();
        if (i == 0) result.winners[0] = 1;
        if (i == 1) result.outcome = OUTCOME_NO_CONTEST;
        if (i == 2) result.n_winners = 2;
        if (i == 3) result.outcome = OUTCOME_TIMEOUT;
        assert(!Rogue_OnMatchEnd(&result));
        assert(!Rogue_IsActive());
        assert(g_rogue_run.phase == ROGUE_PHASE_DEAD);
        assert(g_rogue_run.wins == 0);
    }
    Rogue_ResetRun();
    assert(g_rogue_run.phase == ROGUE_PHASE_NONE);
    assert(g_rogue_run.floor == 0);
    puts("Roguelike state tests passed");
    return 0;
}
