#include "rogue.h"
#include "rogue_state.h"
#include "rogue_route.h"
#include "rogue_debug.h"
#include "rogue_effects.h"
#include "rogue_ai.h"
#include <melee/ft/ftbosslib.h>
#include <melee/ft/fighter.h>
#include <melee/ft/kinds/ftMasterHand/forward.h>
#include <melee/ft/kinds/ftCommon/forward.h>
#include <melee/ft/ft_0D31.h>
#include <melee/ft/kinds/ftFox/ftfoxspecials.h>
#include <melee/ft/types.h>
#include <melee/gr/stage.h>

#include <melee/gm/gm_unsplit.h>
#include <melee/gm/gm_1B03.h>
#include <melee/gm/gm_18A1.h>
#include <melee/gm/gmvs.h>
#include <dolphin/pad.h>
#include <melee/lb/lbaudio_ax.h>
#include <melee/lb/lbarchive.h>
#include <melee/lb/lbdvd.h>
#include <melee/lb/types.h>
#include <melee/mn/types.h>
#include <melee/pl/player.h>
#include <dolphin/vi.h>
#include <stdio.h>
#include <string.h>

static VsModeData encounter_data;
static StartMeleeData start_data;
static MatchExitInfo exit_data;
static CSSData character_select;
static u32 pending_seed;
static bool boss_intro_done;
static u8 controller_port;
int Rogue_ControllerPort(void) { return controller_port; }
static struct { u8 port, stage_index; u16 stage; } stage_intro;

#include <melee/gm/gm_1601.h>
#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmscene.h>
#include <dolphin/os.h>
#include "rogue_ui.h"
#include "rogue_history.h"
static bool resolved;
static int destination;
static void enterCharacterSelect(GameModeState* state)
{
    CSSData* css = gm_GetGameModeStateEnterData(state);
    memset(css, 0, sizeof(*css));
    /* The native single-player CSS layout without Classic's lives/difficulty
     * controls; the run owns those rules. No event-mode callbacks are used. */
    gm_801B06B0(css, EVENT_MATCH, g_rogue_run.player_kind, 3,
                g_rogue_run.player_costume, 0, 4, controller_port);
    lbDvd_SetupVsPreloadCache();
}

static void exitCharacterSelect(GameModeState* state)
{
    CSSData* css = gm_GetGameModeStateExitData(state);
    s8 kind;
    u8 costume;
    if (css->pending_scene_change == CSSPendingSceneChange_2) {
        gm_ChangeGameModeAfterCurrentScene(GM_MENU);
        return;
    }
    gm_801B0730(css, &kind, NULL, &costume, NULL, NULL);
    if (kind < 0 || kind >= CKind_Playable_Count) {
        gm_SetNextGameModeStateId(0);
        return;
    }
    Rogue_NewRun(kind, pending_seed);
    g_rogue_run.player_costume = costume;
    gm_SetNextGameModeStateId(4);
}

static void enterStageIntro(GameModeState* state)
{
    RogueEncounter* encounter = &g_rogue_run.current_encounter;
    (void)state;
    RogueUI_Reset();
    stage_intro.port = controller_port;
    /* Reuse Adventure's stage camera scene over the actual rogue arena.
     * Battlefield/Final Destination cameras fit the compact arena set. */
    stage_intro.stage_index = encounter->stage == St_Kind_Battle ? 10 : 11;
    stage_intro.stage = encounter->stage;
}

static bool in_camp;
static bool in_route;
static bool route_ui_open;
static void campFrame(void)
{
    if (gm_GetFrameCount() >= 60 && RogueUI_CampFrame()) gm_8016B328();
}
static void enterCamp(GameModeState* state)
{
    StartMeleeData* start = gm_GetGameModeStateEnterData(state);
    RogueUI_Reset();
    resolved = false;
    in_camp = true;
    in_route = false;
    RogueEncounter room = g_rogue_run.current_encounter;
    room.enemy_count = 0;
    room.stage = St_Kind_Heal;
    Rogue_SetupEncounter(&encounter_data, &room, g_rogue_run.player_kind);
    *start = encounter_data.start;
    start->players[0].color = g_rogue_run.player_costume;
    start->players[0].slot = controller_port + 1;
    start->players[0].xD_b2 = 1;
    start->rules.x5_0 = 1; /* No elimination/time victory in the rest area. */
    start->rules.x5_1 = 1;
    start->rules.x7 = 9;
    start->rules.on_frame_end = campFrame;
    memset(&exit_data, 0, sizeof(exit_data));
    /* The native room reads these arrays to draw its opponent portraits. */
    memset(&gm_80473A18, 0, sizeof(gm_80473A18));
    memset(gm_80473A18.x76, 33, sizeof(gm_80473A18.x76));
    gm_80473A18._94[0] = 1;
    gm_80473A18._94[1] = g_rogue_run.current_encounter.enemy_count;
    for (int i = 0; i < g_rogue_run.current_encounter.enemy_count; ++i)
        gm_80473A18.x96[i] = g_rogue_run.current_encounter.enemies[i].kind;
    gm_LoadRumbleEnabled(start);
    lbAudioAx_80026F2C(20);
    lbAudioAx_8002702C(4, lbAudioAx_80026E84(g_rogue_run.player_kind));
    lbAudioAx_80027168();
    lbAudioAx_80026F2C(24);
    lbAudioAx_8002702C(8, lbAudioAx_80026EBC(St_Kind_Heal));
    lbAudioAx_80027168();
}

static void exitCamp(GameModeState* state)
{
    (void)state;
    Rogue_LeaveCamp();
    RogueUI_Clear();
    gm_SetNextGameModeStateId(1);
}


static void routeFrame(void)
{
    int choice;

    if (gm_GetFrameCount() < 30)
        return;

    if (!route_ui_open) {
        RogueUI_OpenRoute();
        route_ui_open = true;
    }

    choice = RogueUI_RouteFrame();
    if (choice < 0)
        return;

    if (RogueRoute_Select(&g_rogue_run.route, choice,
                          &g_rogue_run.current_encounter))
    {
        g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
        gm_8016B328();
    }
}

static void enterRoute(GameModeState* state)
{
    StartMeleeData* start = gm_GetGameModeStateEnterData(state);
    const RogueRouteRound* round = RogueRoute_Current(&g_rogue_run.route);
    RogueEncounter room;
    int act;
    int act_floor;
    int i;

    RogueUI_Reset();
    resolved = false;
    in_camp = false;
    in_route = true;
    route_ui_open = false;

    memset(&room, 0, sizeof(room));
    act = (g_rogue_run.floor - 1) / ROGUE_FLOORS_PER_ACT + 1;
    act_floor = (g_rogue_run.floor - 1) % ROGUE_FLOORS_PER_ACT + 1;
    room.act = act;
    room.act_floor = act_floor;
    room.enemy_count = 0;
    room.stage = St_Kind_Heal;
    room.stocks = 3;
    room.name = "Rogue Bracket";

    Rogue_SetupEncounter(&encounter_data, &room, g_rogue_run.player_kind);
    *start = encounter_data.start;
    start->players[0].color = g_rogue_run.player_costume;
    start->players[0].slot = controller_port + 1;
    start->players[0].xD_b2 = 1;
    start->rules.x5_0 = 1;
    start->rules.x5_1 = 1;
    start->rules.x7 = 9;
    start->rules.on_frame_end = routeFrame;

    memset(&exit_data, 0, sizeof(exit_data));
    memset(&gm_80473A18, 0, sizeof(gm_80473A18));
    memset(gm_80473A18.x76, 33, sizeof(gm_80473A18.x76));
    gm_80473A18._94[0] = (u8) act;
    gm_80473A18._94[1] = round && round->generated ? 2 : 0;
    if (round && round->generated) {
        for (i = 0; i < 2; ++i)
            gm_80473A18.x96[i] = round->choices[i].enemy_kind;
    }

    gm_LoadRumbleEnabled(start);
    lbAudioAx_80026F2C(20);
    lbAudioAx_8002702C(4, lbAudioAx_80026E84(g_rogue_run.player_kind));
    lbAudioAx_80027168();
    lbAudioAx_80026F2C(24);
    lbAudioAx_8002702C(8, lbAudioAx_80026EBC(St_Kind_Heal));
    lbAudioAx_80027168();
}

static void exitRoute(GameModeState* state)
{
    (void) state;
    RogueUI_Clear();
    in_route = false;

    if (g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER) {
        gm_ChangeGameModeAfterCurrentScene(GM_MENU);
        return;
    }

    gm_SetNextGameModeStateId(Rogue_BeginCamp() ? 3 : 1);
}

static void encounterFrame(void)
{
    RogueEffects_OnFrame();
    RogueAI_OnFrame();
#ifdef ROGUE_QA
    if (Rogue_DebugMatrixFrame()) {
        gm_8016B328();
        return;
    }
#endif
    if (gm_GetFrameCount() >= 30) RogueUI_HudFrame();
    if (Rogue_IsActive() && g_rogue_run.current_encounter.enemy_kind == CKind_MasterH) {
        HSD_GObj* entity = Player_GetEntity(1);
        Fighter* boss = entity ? entity->user_data : NULL;
        if (boss && gm_GetFrameCount() > 0) {
            if (!boss_intro_done && boss->motion_id != ftMh_MS_Entry) {
                ftBossLib_8015CC14();
                boss->cpu.level = g_rogue_run.current_encounter.cpu_level;
                boss_intro_done = true;
            }
            /* Native Hand death finishes in Sleep without spending a stock.
             * Convert that completed animation into ordinary rogue elimination. */
            if (Player_GetRemainingHP(1) <= 0 && boss->motion_id == ftCo_MS_Sleep)
                Player_SetStocks(1, 0);
        }
    }
}
static void enterEncounter(GameModeState* state)
{
    StartMeleeData* start = gm_GetGameModeStateEnterData(state);
    u64 audio;
    RogueUI_Reset();
    resolved = false;
    destination = 1;
    in_camp = false;
    in_route = false;

    Rogue_SetupEncounter(&encounter_data, &g_rogue_run.current_encounter,
                          g_rogue_run.player_kind);
    /* Do not call gmVsMelee_EnterVs: it imports saved VS preferences. */
    *start = encounter_data.start;
    start->players[0].color = g_rogue_run.player_costume;
    start->players[0].slot = controller_port + 1;
    for (int i = 1; i <= g_rogue_run.current_encounter.enemy_count; ++i)
        if (start->players[i].ckind == start->players[0].ckind &&
            start->players[i].color == start->players[0].color)
            start->players[i].color = start->players[0].color == 0 ? 1 : 0;
    RogueEffects_BeginEncounter();
    boss_intro_done = false;
    RogueAI_Reset();
    start->rules.on_frame_end = encounterFrame;
    memset(&exit_data, 0, sizeof(exit_data));
    gm_SetupSubColors(start);
    gm_LoadRumbleEnabled(start);

    /* Like Debug VS, load each match into fresh fighter/stage heaps.
     * Retaining VS selection caches here keeps relocated archives alive
     * across scene teardown and corrupts the second encounter. */

    audio = lbAudioAx_80026E84(start->players[0].ckind);
    for (int i = 1; i <= g_rogue_run.current_encounter.enemy_count; ++i)
        audio |= lbAudioAx_80026E84(start->players[i].ckind);
    lbAudioAx_80026F2C(20);
    lbAudioAx_8002702C(4, audio);
    lbAudioAx_80027168();
    lbAudioAx_80026F2C(24);
    lbAudioAx_8002702C(8, lbAudioAx_80026EBC(start->rules.stkind));
    lbAudioAx_80027168();
    gm_LoadAnnouncer();
}

bool Rogue_DeveloperBootRequested(void) {
#ifdef ROGUE_QA
    return true;
#else
    return false;
#endif
}
void Rogue_ModeOnLoad(void)
{
    /* The native menu records which controller confirmed the mode, just as
     * Classic and Adventure use for their single-player CSS. Fighter slot 0
     * remains the human; its physical controller port can be any of 0..3. */
    controller_port = gm_801677F0();
    if (controller_port >= 4) controller_port = 0;
#if !(defined(ROGUE_QA) && ROGUE_QA == 3)
    RogueHistory_Load();
#endif
    pending_seed = OSGetTick();
    Rogue_NewRun(CKind_Mario, pending_seed);
    gm_SetGameModeStateId(0);
#ifdef ROGUE_QA
#if ROGUE_QA == 3
    /* Host-driven borrowed-special compatibility matrix. */
    Rogue_DebugMatrixModeLoad();
    gm_SetGameModeStateId(2);
#elif ROGUE_QA == 1
    Rogue_NewRun(CKind_Mario, 314159);
    g_rogue_run.floor=4;g_rogue_run.wins=3;g_rogue_run.currency=180;
    Rogue_GenerateEncounter(&g_rogue_run.current_encounter,&g_rogue_run.rng,4);
    g_rogue_run.phase=ROGUE_PHASE_ENCOUNTER;
    Rogue_BeginCamp();gm_SetGameModeStateId(3);
#else
    Rogue_NewRun(CKind_Mario, 314159);
    RogueRoute_Select(&g_rogue_run.route, 0, &g_rogue_run.current_encounter);
    g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
    gm_SetGameModeStateId(1);
#endif
#endif
}
void Rogue_ModeOnUnload(void)
{
    RogueUI_Reset();
    RogueAI_Reset();
    Rogue_ResetRun();
}
bool Rogue_PostFight(void)
{
#ifdef ROGUE_QA
    /* Matrix cases intentionally terminate/rebuild scenes without awarding wins. */
    if (Rogue_DebugMatrixEnabled()) return false;
#endif
    if (gm_GetCurrentGameMode() != GM_ROGUE || in_camp || in_route ||
        g_rogue_run.phase == ROGUE_PHASE_ROUTE ||
        g_rogue_run.phase == ROGUE_PHASE_REST ||
        g_rogue_run.phase == ROGUE_PHASE_SHOP)
        return false;
    if (!resolved) {
        MatchEnd result;
        int i;
        bool won = Player_GetStocks(0) > 0;
        memset(&result, 0, sizeof(result));
        result.outcome = gmVs_GetSceneController()->state.match_result;
        for (i = 1; i <= g_rogue_run.current_encounter.enemy_count; ++i)
            if (Player_GetStocks(i) > 0) won = false;
        result.is_teams = g_rogue_run.current_encounter.enemy_count > 1;
        result.player_standings[0].pkind = Gm_PKind_Human;
        if (won) {
            result.n_winners = result.n_team_winners = 1;
            result.winners[0] = result.team_winners[0] = 0;
        }
        Rogue_OnMatchEnd(&result);
        resolved = true;
        RogueHistory_Record();
        RogueUI_OpenResults();
    }
    int action = RogueUI_Frame();
    if (action == ROGUE_UI_WAIT) return true;
    if (action == ROGUE_UI_CONTINUE)
        destination = g_rogue_run.phase == ROGUE_PHASE_ROUTE ? 4 :
                      Rogue_BeginCamp() ? 3 : 1;
    else if (action == ROGUE_UI_NEW || action == ROGUE_UI_REPLAY) {
        if (action == ROGUE_UI_NEW) pending_seed = OSGetTick();
        destination = 0;
    } else destination = -1;
    RogueUI_Clear();
    return false;
}
static void exitEncounter(GameModeState* state)
{
    (void)state;
#ifdef ROGUE_QA
    if (Rogue_DebugMatrixConsumeReload()) {
        resolved = false;
        destination = 2;
        gm_SetNextGameModeStateId(2);
        return;
    }
#endif
    if (!resolved) { Rogue_ResetRun(); destination = -1; }
    if (destination < 0) gm_ChangeGameModeAfterCurrentScene(GM_MENU);
    else gm_SetNextGameModeStateId(destination);
}
GameModeState gm_Mode_Rogue_States[] = {
    {
        0, lbDvdPreload_2, 0, enterCharacterSelect, exitCharacterSelect,
        { GS_CSS, &character_select, &character_select },
    },
    {
        1, lbDvdPreload_2, 0, enterStageIntro, NULL,
        { GS_INTRO_NORMAL, &stage_intro, NULL },
    },
    {
        2, lbDvdPreload_2, 0, enterEncounter, exitEncounter,
        { GS_VS, &start_data, &exit_data },
    },
    {
        3, lbDvdPreload_2, 0, enterCamp, exitCamp,
        { GS_VS, &start_data, &exit_data },
    },
    {
        4, lbDvdPreload_2, 0, enterRoute, exitRoute,
        { GS_VS, &start_data, &exit_data },
    },
    { GM_GAMEMODESTATE_TERMINATE },
};
